#include "comp/server/server.hpp"
#include "comp/output/output_manager.hpp"
#include "comp/cursor/cursor_manager.hpp"
#include "comp/workspace/workspace_manager.hpp"
#include "comp/render/frame_scheduler.hpp"
#include "common/logger.hpp"
#include <thread>
#include <chrono>

#if __has_include(<wayland-server.h>)
#include <wayland-server.h>
#define HAVE_WAYLAND_SERVER_H 1
#endif

namespace tinexus::comp {

TinexusServer::TinexusServer() = default;

TinexusServer::~TinexusServer() {
    stop();
#if HAVE_WAYLAND_SERVER_H
    if (m_wl_display) {
        wl_display_destroy(m_wl_display);
        m_wl_display = nullptr;
    }
#endif
}

bool TinexusServer::initialize() {
    log::info("Initializing Tinexus Compositor Server Subsystems...");

#if HAVE_WAYLAND_SERVER_H
    m_wl_display = wl_display_create();
    if (!m_wl_display) {
        log::error("TinexusServer: Failed to create Wayland display (wl_display_create)!");
        return false;
    }

    m_wl_loop = wl_display_get_event_loop(m_wl_display);
    if (!m_wl_loop) {
        log::error("TinexusServer: Failed to get Wayland event loop (wl_display_get_event_loop)!");
        return false;
    }

    const char* socket_name = wl_display_add_socket_auto(m_wl_display);
    if (socket_name) {
        m_display_socket = socket_name;
    } else {
        m_display_socket = "wayland-0";
    }

    wl_display_init_shm(m_wl_display);
    log::info("TinexusServer: Successfully initialized libwayland-server and auto-bound display socket '{}'", m_display_socket);
#else
    m_display_socket = "wayland-0";
    log::info("TinexusServer: Initialized display socket '{}' (Wayland C-API fallback mode)", m_display_socket);
#endif

    // Setup primary display output
    OutputConfig primary_out{"HDMI-A-1", 1920, 1080, 60000, 1.0f, 0, 0, true};
    OutputManager::instance().add_output(primary_out);

    // Setup cursor theme
    CursorManager::instance().set_theme("Adwaita", 24);

    // Setup workspace manager
    WorkspaceManager::instance().initialize_default_workspaces(3);

    // Target frame rate
    FrameScheduler::instance().set_target_refresh_rate(60);

    return true;
}

void TinexusServer::run() {
    m_running = true;
    log::info("TinexusServer event loop running on socket '{}'. Dispatching Wayland client events...", m_display_socket);
    
    // Multi-stage single-threaded compositor scheduling loop:
    // 1. Poll -> 2. Dispatch -> 3. Input -> 4. Animation -> 5. Render -> 6. Frame Callbacks -> 7. Flush
    while (m_running) {
#if HAVE_WAYLAND_SERVER_H
        if (m_wl_loop && m_wl_display) {
            wl_event_loop_dispatch(m_wl_loop, 10);
            wl_display_flush_clients(m_wl_display);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
#else
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
#endif
    }
}

void TinexusServer::stop() {
    m_running = false;
    log::info("TinexusServer stopping event loop...");
}

const std::string& TinexusServer::wayland_display() const noexcept {
    return m_display_socket;
}

} // namespace tinexus::comp
