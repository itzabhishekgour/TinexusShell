#include "comp/server/server.hpp"
#include "comp/backend/backend.hpp"
#include "comp/output/output_manager.hpp"
#include "comp/cursor/cursor_manager.hpp"
#include "comp/workspace/workspace_manager.hpp"
#include "comp/render/frame_scheduler.hpp"
#include "common/logger.hpp"
#include <thread>
#include <chrono>

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

    // wl_shm is now initialized via wlr_shm_create_with_renderer() inside the backend
    log::info("TinexusServer: Successfully initialized wayland server on socket '{}'", m_display_socket);

    // Setup primary display output (Mocked for now until Phase 2B)
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
