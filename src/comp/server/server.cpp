#include "comp/server/server.hpp"
#include "comp/backend/backend.hpp"
#include "comp/output/output_manager.hpp"
#include "comp/cursor/cursor_manager.hpp"
#include "comp/workspace/workspace_manager.hpp"
#include "comp/render/frame_scheduler.hpp"
#include "comp/input/shortcut_engine.hpp"
#include "common/logger.hpp"
#include <thread>
#include <chrono>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <csignal>


#include <wayland-server-core.h>

namespace tinexus::comp {

TinexusServer::TinexusServer() = default;

TinexusServer::~TinexusServer() {
    stop();
    
    // Shutdown backend before destroying display
    if (m_backend) {
        m_backend->shutdown();
        m_backend.reset();
    }

    if (m_wl_display) {
        wl_display_destroy(m_wl_display);
        m_wl_display = nullptr;
    }
}

bool TinexusServer::initialize() {
    log::info("Initializing Tinexus Compositor Server Subsystems...");

    m_wl_display = wl_display_create();
    if (!m_wl_display) {
        log::error("TinexusServer: Failed to create Wayland display!");
        return false;
    }

    m_wl_loop = wl_display_get_event_loop(m_wl_display);
    if (!m_wl_loop) {
        log::error("TinexusServer: Failed to get Wayland event loop!");
        return false;
    }

    // 1. Create Backend (Wlroots by default)
    m_backend = create_wlroots_backend(m_wl_display);
    if (!m_backend) {
        log::error("TinexusServer: Failed to instantiate backend.");
        return false;
    }

    // 2. Initialize backend (this creates wlr_backend, renderer, allocator, etc)
    if (!m_backend->initialize()) {
        log::error("TinexusServer: Failed to initialize backend.");
        return false;
    }

    // 3. Add Wayland socket
    const char* socket_name = wl_display_add_socket_auto(m_wl_display);
    if (socket_name) {
        m_display_socket = socket_name;
    } else {
        m_display_socket = "wayland-0";
    }

    // Export WAYLAND_DISPLAY so child processes (launcher, etc.) can connect
    setenv("WAYLAND_DISPLAY", m_display_socket.c_str(), 1);
    log::info("TinexusServer: WAYLAND_DISPLAY={}", m_display_socket);

    // wl_shm is now initialized via wlr_shm_create_with_renderer() inside the backend
    log::info("TinexusServer: Successfully initialized wayland server on socket '{}'", m_display_socket);

    // Setup primary display output (Mocked for now until Phase 2B)
    OutputConfig primary_out{"HDMI-A-1", 1920, 1080, 60000, 1.0f, 0, 0, true};
    OutputManager::instance().add_output(primary_out);

    // Setup cursor theme
    CursorManager::instance().set_theme("Adwaita", 24);

    // Setup workspace manager with 9 workspaces (Super+1–9)
    WorkspaceManager::instance().initialize_default_workspaces(9);

    // Target frame rate
    FrameScheduler::instance().set_target_refresh_rate(60);

    // ── Global shortcut handler ───────────────────────────────────────────────
    ShortcutEngine::instance().set_shortcut_callback(
        [this](const std::string& shortcut_name) {

            // ── Launcher ──────────────────────────────────────────────────
            if (shortcut_name == "launcher_toggle") {
                // SECURITY: never spawn launcher while screen is locked
                if (m_backend->is_locked()) {
                    log::warn("[Server] Launcher blocked — screen is locked.");
                    return;
                }
                log::info("[Server] Ctrl+K: Sending SHORTCUT_ACTIVATED to ipcd");
                int sock = socket(AF_UNIX, SOCK_STREAM, 0);
                if (sock >= 0) {
                    struct sockaddr_un addr;
                    memset(&addr, 0, sizeof(addr));
                    addr.sun_family = AF_UNIX;
                    char path[256];
                    snprintf(path, sizeof(path), "/run/user/%d/tinexus/ipc.sock", getuid());
                    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
                    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
#pragma pack(push, 1)
                        struct IpcHeader {
                            uint32_t magic = 0x544E5853;
                            uint16_t version = 0x0100;
                            uint16_t msg_type;
                            uint16_t flags = 0;
                            uint32_t sequence_id = 0;
                            uint32_t payload_len = 0;
                            uint32_t checksum = 0;
                        };
#pragma pack(pop)
                        IpcHeader msg1;
                        msg1.msg_type = 1005; // SHORTCUT_ACTIVATED
                        send(sock, &msg1, sizeof(msg1), MSG_NOSIGNAL);

                        IpcHeader msg2;
                        msg2.msg_type = 10; // SYS_PING
                        send(sock, &msg2, sizeof(msg2), MSG_NOSIGNAL);

                        // Block until we get PONG to ensure ipcd read the queue
                        IpcHeader rx;
                        recv(sock, &rx, sizeof(rx), 0);
                    }
                    close(sock);
                }
                return;
            }

            // ── Lock screen: Super+L ─────────────────────────────────────────
            if (shortcut_name == "lock_screen") {
                if (m_backend->is_locked()) {
                    log::info("[Server] Already locked.");
                    return;
                }
                log::info("[Server] Super+L: spawning tinexus-lock, marking session LOCKED");
                m_backend->set_locked(true);
                pid_t pid = fork();
                if (pid < 0) { log::error("[Server] fork() failed for lock"); m_backend->set_locked(false); return; }
                if (pid == 0) {
                    pid_t grandchild = fork();
                    if (grandchild < 0) { _exit(1); }
                    if (grandchild == 0) {
                        setenv("WAYLAND_DISPLAY", m_display_socket.c_str(), 1);
                        setsid();
                        execlp("tinexus-lock", "tinexus-lock", nullptr);
                        execl("/usr/bin/tinexus-lock", "tinexus-lock", nullptr);
                        _exit(127);
                    }
                    _exit(0);
                }
                // Wait for the intermediate child; the grandchild (lock) is now orphaned
                int status = 0;
                waitpid(pid, &status, 0);
                return;
            }

            // ── Workspace switching: Super+1–9 ────────────────────────────────
            if (shortcut_name.rfind("workspace_switch_", 0) == 0) {
                uint32_t ws_num = static_cast<uint32_t>(
                    std::stoul(shortcut_name.substr(17))); // skip "workspace_switch_"
                log::info("[Server] Switching to workspace {}", ws_num);
                WorkspaceManager::instance().switch_workspace(ws_num);
                return;
            }

            // ── Window snapping (wired to focused window in future) ───────────
            if (shortcut_name == "snap_left") {
                log::info("[Server] snap_left — window snapping (Phase B)");
                return;
            }
            if (shortcut_name == "snap_right") {
                log::info("[Server] snap_right — window snapping (Phase B)");
                return;
            }
            if (shortcut_name == "maximize") {
                log::info("[Server] maximize — window maximize (Phase B)");
                return;
            }
            if (shortcut_name == "restore") {
                log::info("[Server] restore — window restore (Phase B)");
                return;
            }
            if (shortcut_name == "close_window") {
                log::info("[Server] close_window — (Phase B)");
                return;
            }

            // ── Alt+Tab ───────────────────────────────────────────────────────
            if (shortcut_name == "alttab_next" || shortcut_name == "alttab_prev") {
                log::info("[Server] {} — window switcher (Phase B)", shortcut_name);
                return;
            }

            // ── Workspace overview (Mission Control): Super+Tab ───────────────
            if (shortcut_name == "workspace_overview_toggle") {
                log::info("[Server] Toggling workspace overview (Mission Control)");
                WorkspaceManager::instance().toggle_overview();
                return;
            }

            log::warn("[Server] Unknown shortcut: {}", shortcut_name);
        });
    log::info("[Server] ShortcutEngine registered: Ctrl+K, Super+1–9, Super+L, Super+Arrows, Alt+Tab");

    return true;
}

void TinexusServer::run() {
    log::info("TinexusServer: Starting backend...");
    if (!m_backend->start()) {
        log::error("TinexusServer: Failed to start backend.");
        return;
    }

    m_running = true;
    log::info("TinexusServer event loop running on socket '{}'. Dispatching Wayland client events...", m_display_socket);
    
    // Hand over control to the Wayland event loop
    wl_display_run(m_wl_display);
}

void TinexusServer::stop() {
    if (m_running) {
        m_running = false;
        log::info("TinexusServer stopping event loop...");
        if (m_wl_display) {
            wl_display_terminate(m_wl_display);
        }
        if (m_backend) {
            m_backend->stop();
        }
    }
}

const std::string& TinexusServer::wayland_display() const noexcept {
    return m_display_socket;
}

} // namespace tinexus::comp
