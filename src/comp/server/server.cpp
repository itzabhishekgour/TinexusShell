#include "comp/server/server.hpp"
#include "comp/window/RestoreAnimation.hpp"
#include "comp/backend/backend.hpp"
#include "comp/output/output_manager.hpp"
#include "comp/cursor/cursor_manager.hpp"
#include "comp/workspace/workspace_manager.hpp"
#include "comp/render/frame_scheduler.hpp"
#include "comp/input/shortcut_engine.hpp"
#include "comp/input/seat_manager.hpp"
#include "common/logger.hpp"
#include "common/RuntimePaths.hpp"
#include "common/AudioUtils.hpp"
#include "common/BacklightUtils.hpp"
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
#include <fcntl.h>
#include <sys/stat.h>
#include <poll.h>

#include "ipcd/protocol/header.hpp"
#include "ipcd/protocol/dock_protocol.hpp"
#include "comp/window/window_manager.hpp"
#include "comp/window/MinimizeAnimation.hpp"
#include "comp/focus/focus_manager.hpp"
#include "comp/window/scene_graph.hpp"

#include <wayland-server-core.h>

static int handle_cmd_fifo(int fd, uint32_t mask, void* data) {
    if (mask & WL_EVENT_READABLE) {
        auto* backend = static_cast<tinexus::comp::Backend*>(data);
        char buf[256];
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            std::string cmd(buf);
            if (cmd.starts_with("focus ")) {
                std::string app_id = cmd.substr(6);
                while (!app_id.empty() && (app_id.back() == '\n' || app_id.back() == '\r')) {
                    app_id.pop_back();
                }
                backend->focus_app(app_id);
            } else if (cmd.starts_with("click")) {
                tinexus::log::info("[Server] Simulating pointer click via FIFO");
                tinexus::comp::SeatManager::instance().notify_button(0, 0x110, 1);
                tinexus::comp::SeatManager::instance().notify_button(0, 0x110, 0);
            } else if (cmd.starts_with("screenshot ")) {
                std::string path = cmd.substr(11);
                while (!path.empty() && (path.back() == '\n' || path.back() == '\r' || path.back() == ' ')) {
                    path.pop_back();
                }
                tinexus::log::info("[Server] Requested screenshot to '{}'", path);
                backend->dump_screenshot(path);
            } else if (cmd.starts_with("warp ")) {
                double x = 0, y = 0;
                if (sscanf(cmd.c_str() + 5, "%lf %lf", &x, &y) == 2) {
                    backend->warp_cursor(x, y);
                }
            } else if (cmd.starts_with("click_ssd")) {
                tinexus::log::info("[Server] Simulating click at cursor position via FIFO");
                backend->simulate_click(0x110, 1);
                backend->simulate_click(0x110, 0);
            }
        }
    }
    return 1;
}

namespace tinexus::comp {

static TinexusServer* s_instance = nullptr;

TinexusServer* TinexusServer::instance() {
    return s_instance;
}

TinexusServer::TinexusServer() {
    s_instance = this;
}

TinexusServer::~TinexusServer() {
    if (s_instance == this) s_instance = nullptr;
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

    // 1. Initialize workspaces before backend so per-workspace scene trees are created
    WorkspaceManager::instance().initialize_default_workspaces(9);

    // 2. Create Backend (Wlroots by default)
    m_backend = create_wlroots_backend(m_wl_display);
    if (!m_backend) {
        log::error("TinexusServer: Failed to instantiate backend.");
        return false;
    }

    // 3. Initialize backend (this creates wlr_backend, renderer, allocator, etc)
    if (!m_backend->initialize()) {
        log::error("TinexusServer: Failed to initialize backend.");
        return false;
    }

    // Initialize FIFO command socket for simple IPC (e.g., focus requests)
    tinexus::common::RuntimePaths::ensure_runtime_dir();
    std::string fifo_path = tinexus::common::RuntimePaths::get_comp_fifo_path();
    unlink(fifo_path.c_str());
    mkfifo(fifo_path.c_str(), 0600);
    int fifo_fd = open(fifo_path.c_str(), O_RDWR | O_NONBLOCK);
    if (fifo_fd >= 0) {
        wl_event_loop_add_fd(m_wl_loop, fifo_fd, WL_EVENT_READABLE, handle_cmd_fifo, m_backend.get());
    }

    // Ensure XDG_RUNTIME_DIR is properly configured and directory exists with mode 0700
    const char* xdg_runtime = getenv("XDG_RUNTIME_DIR");
    if (!xdg_runtime || !*xdg_runtime) {
        xdg_runtime = "/run/user/1000";
        setenv("XDG_RUNTIME_DIR", xdg_runtime, 1);
    }
    struct stat st_xdg{};
    if (stat(xdg_runtime, &st_xdg) != 0) {
        mkdir(xdg_runtime, 0700);
    } else {
        chmod(xdg_runtime, 0700);
    }

    // Clean up stale lock and socket if they exist
    std::string lock_file = std::string(xdg_runtime) + "/wayland-0.lock";
    std::string sock_file = std::string(xdg_runtime) + "/wayland-0";
    unlink(lock_file.c_str());
    unlink(sock_file.c_str());

    // Also support legacy /tinexus_comp_cmd in XDG_RUNTIME_DIR for backwards compatibility
    std::string legacy_path = std::string(xdg_runtime) + "/tinexus_comp_cmd";
    unlink(legacy_path.c_str());
    symlink(fifo_path.c_str(), legacy_path.c_str());

    // 3. Add Wayland socket
    const char* socket_name = wl_display_add_socket_auto(m_wl_display);
    if (!socket_name) {
        log::warn("TinexusServer: wl_display_add_socket_auto failed ({}), trying explicit wl_display_add_socket('wayland-0')...", strerror(errno));
        if (wl_display_add_socket(m_wl_display, "wayland-0") == 0) {
            socket_name = "wayland-0";
        }
    }

    if (!socket_name) {
        log::error("TinexusServer: FATAL — Failed to add Wayland socket in '{}': {}", xdg_runtime, strerror(errno));
        return false;
    }

    m_display_socket = socket_name;

    // Export WAYLAND_DISPLAY so child processes (launcher, etc.) can connect
    setenv("WAYLAND_DISPLAY", m_display_socket.c_str(), 1);
    log::info("TinexusServer: WAYLAND_DISPLAY={}", m_display_socket);

    // Verify socket file existence on disk and grant read/write access
    std::string verified_sock_path = std::string(xdg_runtime) + "/" + m_display_socket;
    struct stat st_sock{};
    if (stat(verified_sock_path.c_str(), &st_sock) == 0) {
        chmod(verified_sock_path.c_str(), 0666);
        log::info("TinexusServer: Verified Wayland socket at '{}' (mode=0666)", verified_sock_path);
    } else {
        log::warn("TinexusServer: Socket stat check on '{}' returned: {}", verified_sock_path, strerror(errno));
    }

    // wl_shm is now initialized via wlr_shm_create_with_renderer() inside the backend
    log::info("TinexusServer: Successfully initialized wayland server on socket '{}'", m_display_socket);

    // Setup cursor theme
    CursorManager::instance().set_theme("Adwaita", 24);

    // ── Global shortcut handler ───────────────────────────────────────────────
    ShortcutEngine::instance().set_shortcut_callback(
        [this](const std::string& shortcut_name) {

            if (shortcut_name == "launcher_toggle") {
                // SECURITY: never spawn launcher while screen is locked
                if (m_backend->is_locked()) {
                    log::warn("[Server] Launcher blocked — screen is locked.");
                    return;
                }
                if (m_backend->toggle_launcher()) {
                    log::info("[Server] Ctrl+K: Toggled active launcher");
                    return;
                }
                log::info("[Server] Ctrl+K: Spawning tinexus-launcher directly");
                pid_t pid = fork();
                if (pid < 0) { log::error("[Server] fork() failed for launcher"); return; }
                if (pid == 0) {
                    pid_t grandchild = fork();
                    if (grandchild < 0) { _exit(1); }
                    if (grandchild == 0) {
                        // Close inherited file descriptors (3..255) to prevent socket/DRM leaks
                        for (int fd = 3; fd < 256; ++fd) {
                            ::close(fd);
                        }
                        const char* rundir = getenv("XDG_RUNTIME_DIR");
                        if (!rundir || !*rundir) {
                            rundir = "/run/user/0";
                        }
                        setenv("XDG_RUNTIME_DIR", rundir, 1);
                        setenv("WAYLAND_DISPLAY", m_display_socket.c_str(), 1);
                        setenv("QT_QPA_PLATFORM", "wayland", 1);
                        setenv("QT_WAYLAND_SHELL_INTEGRATION", "layer-shell", 1);
                        setenv("QT_PLUGIN_PATH", "/usr/lib/x86_64-linux-gnu/qt6/plugins", 0);
                        setenv("QML_IMPORT_PATH", "/usr/lib/x86_64-linux-gnu/qt6/qml:/usr/share/tinexus", 0);
                        setenv("QML2_IMPORT_PATH", "/usr/lib/x86_64-linux-gnu/qt6/qml:/usr/share/tinexus", 0);
                        const char* home = std::getenv("HOME");
                        if (home) {
                            std::string lib_path = std::string(home) + "/tinexus/build/debug/src/common";
                            const char* old_ld = std::getenv("LD_LIBRARY_PATH");
                            std::string new_ld = old_ld ? lib_path + ":" + old_ld : lib_path;
                            setenv("LD_LIBRARY_PATH", new_ld.c_str(), 0);
                        }
                        setsid();
                        execlp("tinexus-launcher", "tinexus-launcher", nullptr);
                        execl("/usr/bin/tinexus-launcher", "tinexus-launcher", nullptr);
                        _exit(127);
                    }
                    _exit(0);
                }
                // Wait for the intermediate child; the grandchild (launcher) is now orphaned
                int status = 0;
                waitpid(pid, &status, 0);
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

            // ── Window snapping & maximize/restore ───────────────────────────
            if (shortcut_name == "snap_left") {
                log::info("[Server] snap_left — snapping focused window to left half");
                m_backend->snap_active_window(SnapMode::Left);
                return;
            }
            if (shortcut_name == "snap_right") {
                log::info("[Server] snap_right — snapping focused window to right half");
                m_backend->snap_active_window(SnapMode::Right);
                return;
            }
            if (shortcut_name == "maximize") {
                log::info("[Server] maximize — maximizing focused window");
                m_backend->maximize_active_window();
                return;
            }
            if (shortcut_name == "restore") {
                log::info("[Server] restore — restoring focused window");
                m_backend->restore_active_window();
                return;
            }
            if (shortcut_name == "close_window") {
                log::info("[Server] close_window — closing focused window");
                m_backend->close_active_window();
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

            // ── Multimedia Shortcuts ──────────────────────────────────────────
            if (shortcut_name == "volume_up") {
                int vol = tinexus::hardware::AudioUtils::step_volume(+5);
                log::info("[Server] Volume stepped up to {}%", vol);
                return;
            }
            if (shortcut_name == "volume_down") {
                int vol = tinexus::hardware::AudioUtils::step_volume(-5);
                log::info("[Server] Volume stepped down to {}%", vol);
                return;
            }
            if (shortcut_name == "volume_mute") {
                bool muted = tinexus::hardware::AudioUtils::toggle_mute();
                log::info("[Server] Volume mute toggled: {}", muted);
                return;
            }
            if (shortcut_name == "brightness_up") {
                int bl = tinexus::hardware::BacklightUtils::step_brightness(+5);
                log::info("[Server] Brightness stepped up to {}%", bl);
                return;
            }
            if (shortcut_name == "brightness_down") {
                int bl = tinexus::hardware::BacklightUtils::step_brightness(-5);
                log::info("[Server] Brightness stepped down to {}%", bl);
                return;
            }

            log::warn("[Server] Unknown shortcut: {}", shortcut_name);
        });
    log::info("[Server] ShortcutEngine registered: Ctrl+K, Super+1–9, Super+L, Super+Arrows, Alt+Tab, Volume/Brightness keys");

    setup_ipc_connection();

    return true;
}

bool TinexusServer::run() {
    log::info("TinexusServer: Starting backend...");
    if (!m_backend->start()) {
        log::error("TinexusServer: Failed to start backend.");
        return false;
    }

    m_running = true;
    log::info("TinexusServer event loop running on socket '{}'. Dispatching Wayland client events...", m_display_socket);
    
    // Hand over control to the Wayland event loop
    wl_display_run(m_wl_display);
    return true;
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

[[maybe_unused]] static int s_icon_query_timeout_handler(void* data) {
    auto* srv = static_cast<TinexusServer*>(data);
    srv->check_icon_query_timeout();
    return 0;
}

void TinexusServer::setup_ipc_connection() {
    m_ipc_socket = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (m_ipc_socket >= 0) {
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        std::string sock_path = tinexus::common::RuntimePaths::get_ipc_socket_path();
        strncpy(addr.sun_path, sock_path.c_str(), sizeof(addr.sun_path) - 1);
        if (connect(m_ipc_socket, (struct sockaddr*)&addr, sizeof(addr)) == 0 || errno == EINPROGRESS) {
            wl_event_loop_add_fd(m_wl_loop, m_ipc_socket, WL_EVENT_READABLE, handle_ipc_fd, this);
            log::info("[Server] Connected to ipcd at {}", addr.sun_path);
            
            // Subscribe to dock messages
            uint16_t sub_types[] = {
                static_cast<uint16_t>(tinexus::ipcd::protocol::DockMessageType::DOCK_ICON_POSITION),
                static_cast<uint16_t>(tinexus::ipcd::protocol::DockMessageType::DOCK_RESTORE_REQUEST),
                static_cast<uint16_t>(tinexus::ipcd::protocol::DockMessageType::DOCK_RAISE_AND_FOCUS)
            };
            
            for (uint16_t t : sub_types) {
                struct {
                    tinexus::ipcd::protocol::Header hdr;
                    uint16_t topic;
                } __attribute__((packed)) msg;
                msg.hdr.magic = tinexus::ipcd::protocol::TINEXUS_IPC_MAGIC;
                msg.hdr.version = tinexus::ipcd::protocol::TINEXUS_IPC_VERSION_1;
                msg.hdr.msg_type = static_cast<uint16_t>(tinexus::ipcd::protocol::MessageType::SYS_SUBSCRIBE_TOPIC);
                msg.hdr.payload_len = sizeof(msg.topic);
                msg.topic = t;
                send(m_ipc_socket, &msg, sizeof(msg), MSG_NOSIGNAL);
            }
        } else {
            close(m_ipc_socket);
            m_ipc_socket = -1;
            log::warn("[Server] Failed to connect to ipcd persistent socket");
        }
    }
}

int TinexusServer::handle_ipc_fd(int fd, uint32_t mask, void* data) {
    auto* srv = static_cast<TinexusServer*>(data);
    if (mask & (WL_EVENT_ERROR | WL_EVENT_HANGUP)) {
        log::warn("[Server] IPC socket error/hangup");
        return 0;
    }
    
    tinexus::ipcd::protocol::Header hdr;
    ssize_t n = recv(fd, &hdr, sizeof(hdr), MSG_PEEK);
    if (n == sizeof(hdr)) {
        if (hdr.magic != tinexus::ipcd::protocol::TINEXUS_IPC_MAGIC) {
            // drop bad byte
            char c;
            recv(fd, &c, 1, 0);
            return 1;
        }
        std::vector<uint8_t> payload(hdr.payload_len);
        // read full message
        n = recv(fd, nullptr, 0, MSG_PEEK); // just to check if data is available
        // Need to read hdr + payload
        std::vector<uint8_t> buf(sizeof(hdr) + hdr.payload_len);
        n = recv(fd, buf.data(), buf.size(), MSG_DONTWAIT);
        if (n == static_cast<ssize_t>(buf.size())) {
            srv->process_ipc_message(hdr.msg_type, buf.data() + sizeof(hdr), hdr.payload_len);
        }
    } else if (n <= 0 && errno != EAGAIN) {
        log::warn("[Server] IPC socket closed");
        close(fd);
        srv->m_ipc_socket = -1;
    }
    return 1;
}

void TinexusServer::process_ipc_message(uint16_t msg_type, const void* payload, uint32_t payload_len) {
    using namespace tinexus::ipcd::protocol;
    if (msg_type == static_cast<uint16_t>(DockMessageType::DOCK_ICON_POSITION) && payload_len >= sizeof(DockIconPositionPayload)) {
        const auto* p = static_cast<const DockIconPositionPayload*>(payload);
        if (!m_pending_icon_query.active || m_pending_icon_query.app_id != p->app_id) {
            log::warn("[Comp] DOCK_ICON_POSITION stale or mismatch — discarded");
            return;
        }
        m_pending_icon_query.active = false;
        auto win = WindowManager::instance().find_window(m_pending_icon_query.surface_id);
        if (win && win->animation_phase == AnimationPhase::None) {
            win->dock_icon_x = p->x + p->w / 2;
            win->dock_icon_y = p->y;
            win->animation_phase = AnimationPhase::Minimizing;
            win->active_dock_anim = std::make_unique<MinimizeAnimation>(win, 1.0f, 1.0f, win->saved_x, win->saved_y);
            win->active_dock_anim->start();
        }
    }
    else if (msg_type == static_cast<uint16_t>(DockMessageType::DOCK_RESTORE_REQUEST) && payload_len >= sizeof(DockNotifyPayload)) {
        const auto* p = static_cast<const DockNotifyPayload*>(payload);
        if (m_backend) {
            m_backend->restore_window_by_app_id(p->app_id);
        }
        auto win = WindowManager::instance().find_window(p->surface_id);
        if (win) {
            float start_opacity = 0.0f;
            float start_scale = 0.1f;
            int32_t start_x = win->dock_icon_x;
            int32_t start_y = win->dock_icon_y;

            if (win->animation_phase == AnimationPhase::Restoring) return; // Ignore duplicate
            if (win->animation_phase == AnimationPhase::Minimizing) {
                // Mid-flight reversal
                start_opacity = win->opacity;
                start_scale = win->scale;
                start_x = win->x;
                start_y = win->y;
                win->active_dock_anim.reset();
            }

            win->animation_phase = AnimationPhase::Restoring;
            win->active_dock_anim = std::make_unique<RestoreAnimation>(
                win, start_opacity, start_scale, start_x, start_y
            );
            win->active_dock_anim->start();
        }
    }
    else if (msg_type == static_cast<uint16_t>(DockMessageType::DOCK_RAISE_AND_FOCUS) && payload_len >= sizeof(DockNotifyPayload)) {
        const auto* p = static_cast<const DockNotifyPayload*>(payload);
        if (m_backend) {
            m_backend->focus_app(p->app_id);
        }
        
        if (m_ipc_socket < 0) {
            setup_ipc_connection();
        }
        if (m_ipc_socket >= 0) {
            struct {
                Header hdr;
                DockFocusChangedPayload pld;
            } __attribute__((packed)) msg;
            msg.hdr.magic = TINEXUS_IPC_MAGIC;
            msg.hdr.version = TINEXUS_IPC_VERSION_1;
            msg.hdr.msg_type = static_cast<uint16_t>(DockMessageType::DOCK_NOTIFY_FOCUS_CHANGED);
            msg.hdr.payload_len = sizeof(msg.pld);
            strncpy(msg.pld.app_id, p->app_id, sizeof(msg.pld.app_id)-1);
            msg.pld.is_focused = 1;
            send(m_ipc_socket, &msg, sizeof(msg), MSG_NOSIGNAL);
        }
    }
}

void TinexusServer::check_icon_query_timeout() {
    if (!m_pending_icon_query.active) return;
    
    log::warn("[Comp] DOCK_QUERY_ICON_POSITION timed out — using bottom-center fallback");
    m_pending_icon_query.active = false;
    auto win = WindowManager::instance().find_window(m_pending_icon_query.surface_id);
    if (win && win->animation_phase == AnimationPhase::None) {
        // fallback: bottom-center of window geometry
        int32_t fallback_x = win->saved_x + static_cast<int32_t>(win->width > 0 ? win->width / 2 : 400);
        int32_t fallback_y = win->saved_y + static_cast<int32_t>(win->height > 0 ? win->height : 600);
        win->dock_icon_x = fallback_x;
        win->dock_icon_y = fallback_y;
        win->animation_phase = AnimationPhase::Minimizing;
        win->active_dock_anim = std::make_unique<MinimizeAnimation>(win, 1.0f, 1.0f, win->saved_x, win->saved_y);
        win->active_dock_anim->start();
    }
}

void TinexusServer::trigger_minimize(uint64_t surface_id) {
    auto win = WindowManager::instance().find_window(surface_id);
    if (!win) return;

    if (win->animation_phase == AnimationPhase::Minimizing) return;
    if (win->animation_phase == AnimationPhase::Restoring) {
        // Reverse mid-flight -> Minimize
        win->active_dock_anim.reset();
        win->animation_phase = AnimationPhase::Minimizing;
        // Bypass IPC query, use cached icon position
        win->active_dock_anim = std::make_unique<MinimizeAnimation>(
            win, win->opacity, win->scale, win->x, win->y
        );
        win->active_dock_anim->start();
        return;
    }

    // Fresh minimize
    m_pending_icon_query.active = true;
    m_pending_icon_query.surface_id = surface_id;
    m_pending_icon_query.app_id = win->toplevel.app_id();
    m_pending_icon_query.deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);

    if (m_ipc_socket < 0) {
        setup_ipc_connection();
    }
    if (m_ipc_socket >= 0) {
        struct {
            tinexus::ipcd::protocol::Header hdr;
            tinexus::ipcd::protocol::DockQueryIconPositionPayload pld;
        } __attribute__((packed)) msg;
        msg.hdr.magic = tinexus::ipcd::protocol::TINEXUS_IPC_MAGIC;
        msg.hdr.version = tinexus::ipcd::protocol::TINEXUS_IPC_VERSION_1;
        msg.hdr.msg_type = static_cast<uint16_t>(tinexus::ipcd::protocol::DockMessageType::DOCK_QUERY_ICON_POSITION);
        msg.hdr.payload_len = sizeof(msg.pld);
        strncpy(msg.pld.app_id, win->toplevel.app_id().c_str(), sizeof(msg.pld.app_id) - 1);
        send(m_ipc_socket, &msg, sizeof(msg), MSG_NOSIGNAL);
    }
}

void TinexusServer::notify_window_minimized(const std::string& app_id) {
    if (m_ipc_socket < 0) setup_ipc_connection();
    if (m_ipc_socket >= 0) {
        struct {
            tinexus::ipcd::protocol::Header hdr;
            tinexus::ipcd::protocol::DockNotifyPayload pld;
        } __attribute__((packed)) msg;
        msg.hdr.magic = tinexus::ipcd::protocol::TINEXUS_IPC_MAGIC;
        msg.hdr.version = tinexus::ipcd::protocol::TINEXUS_IPC_VERSION_1;
        msg.hdr.msg_type = static_cast<uint16_t>(tinexus::ipcd::protocol::DockMessageType::DOCK_NOTIFY_MINIMIZED);
        msg.hdr.payload_len = sizeof(msg.pld);
        msg.hdr.sequence_id = 0;
        msg.hdr.flags = 0;
        msg.hdr.checksum = 0;
        strncpy(msg.pld.app_id, app_id.c_str(), sizeof(msg.pld.app_id) - 1);
        msg.pld.surface_id = 0;
        send(m_ipc_socket, &msg, sizeof(msg), MSG_NOSIGNAL);
    }
}

void TinexusServer::notify_window_restored(const std::string& app_id) {
    if (m_ipc_socket < 0) setup_ipc_connection();
    if (m_ipc_socket >= 0) {
        struct {
            tinexus::ipcd::protocol::Header hdr;
            tinexus::ipcd::protocol::DockNotifyPayload pld;
        } __attribute__((packed)) msg;
        msg.hdr.magic = tinexus::ipcd::protocol::TINEXUS_IPC_MAGIC;
        msg.hdr.version = tinexus::ipcd::protocol::TINEXUS_IPC_VERSION_1;
        msg.hdr.msg_type = static_cast<uint16_t>(tinexus::ipcd::protocol::DockMessageType::DOCK_NOTIFY_RESTORED);
        msg.hdr.payload_len = sizeof(msg.pld);
        msg.hdr.sequence_id = 0;
        msg.hdr.flags = 0;
        msg.hdr.checksum = 0;
        strncpy(msg.pld.app_id, app_id.c_str(), sizeof(msg.pld.app_id) - 1);
        msg.pld.surface_id = 0;
        send(m_ipc_socket, &msg, sizeof(msg), MSG_NOSIGNAL);
    }
}

void TinexusServer::notify_app_started(const std::string& app_id) {
    if (m_ipc_socket < 0) setup_ipc_connection();
    if (m_ipc_socket >= 0) {
        struct {
            tinexus::ipcd::protocol::Header hdr;
            tinexus::ipcd::protocol::DockNotifyPayload pld;
        } __attribute__((packed)) msg;
        msg.hdr.magic = tinexus::ipcd::protocol::TINEXUS_IPC_MAGIC;
        msg.hdr.version = tinexus::ipcd::protocol::TINEXUS_IPC_VERSION_1;
        msg.hdr.msg_type = static_cast<uint16_t>(tinexus::ipcd::protocol::DockMessageType::DOCK_NOTIFY_APP_STARTED);
        msg.hdr.payload_len = sizeof(msg.pld);
        msg.hdr.sequence_id = 0;
        msg.hdr.flags = 0;
        msg.hdr.checksum = 0;
        strncpy(msg.pld.app_id, app_id.c_str(), sizeof(msg.pld.app_id) - 1);
        msg.pld.surface_id = 0;
        send(m_ipc_socket, &msg, sizeof(msg), MSG_NOSIGNAL);
    }
}

void TinexusServer::notify_app_closed(const std::string& app_id) {
    if (m_ipc_socket < 0) setup_ipc_connection();
    if (m_ipc_socket >= 0) {
        struct {
            tinexus::ipcd::protocol::Header hdr;
            tinexus::ipcd::protocol::DockNotifyPayload pld;
        } __attribute__((packed)) msg;
        msg.hdr.magic = tinexus::ipcd::protocol::TINEXUS_IPC_MAGIC;
        msg.hdr.version = tinexus::ipcd::protocol::TINEXUS_IPC_VERSION_1;
        msg.hdr.msg_type = static_cast<uint16_t>(tinexus::ipcd::protocol::DockMessageType::DOCK_NOTIFY_APP_CLOSED);
        msg.hdr.payload_len = sizeof(msg.pld);
        msg.hdr.sequence_id = 0;
        msg.hdr.flags = 0;
        msg.hdr.checksum = 0;
        strncpy(msg.pld.app_id, app_id.c_str(), sizeof(msg.pld.app_id) - 1);
        msg.pld.surface_id = 0;
        send(m_ipc_socket, &msg, sizeof(msg), MSG_NOSIGNAL);
    }
}

void TinexusServer::notify_app_focus_changed(const std::string& app_id, uint8_t is_focused) {
    if (m_ipc_socket < 0) setup_ipc_connection();
    if (m_ipc_socket >= 0) {
        struct {
            tinexus::ipcd::protocol::Header hdr;
            tinexus::ipcd::protocol::DockFocusChangedPayload pld;
        } __attribute__((packed)) msg;
        msg.hdr.magic = tinexus::ipcd::protocol::TINEXUS_IPC_MAGIC;
        msg.hdr.version = tinexus::ipcd::protocol::TINEXUS_IPC_VERSION_1;
        msg.hdr.msg_type = static_cast<uint16_t>(tinexus::ipcd::protocol::DockMessageType::DOCK_NOTIFY_FOCUS_CHANGED);
        msg.hdr.payload_len = sizeof(msg.pld);
        msg.hdr.sequence_id = 0;
        msg.hdr.flags = 0;
        msg.hdr.checksum = 0;
        strncpy(msg.pld.app_id, app_id.c_str(), sizeof(msg.pld.app_id) - 1);
        msg.pld.is_focused = is_focused;
        send(m_ipc_socket, &msg, sizeof(msg), MSG_NOSIGNAL);
    }
}

} // namespace tinexus::comp
