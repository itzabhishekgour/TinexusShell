#include "comp/server/server.hpp"
#include "comp/output/output_manager.hpp"
#include "comp/cursor/cursor_manager.hpp"
#include "comp/workspace/workspace_manager.hpp"
#include "comp/render/frame_scheduler.hpp"
#include "common/logger.hpp"

namespace tinexus::comp {

TinexusServer::TinexusServer() = default;
TinexusServer::~TinexusServer() = default;

bool TinexusServer::initialize() {
    log::info("Initializing Tinexus Compositor Server Subsystems...");

    // Setup primary display output
    OutputConfig primary_out{"HDMI-A-1", 1920, 1080, 60000, 1.0f, 0, 0, true};
    OutputManager::instance().add_output(primary_out);

    // Setup cursor theme
    CursorManager::instance().set_theme("Adwaita", 24);

    // Setup workspace manager
    WorkspaceManager::instance().initialize_default_workspaces(3);

    // Target frame rate
    FrameScheduler::instance().set_target_refresh_rate(60);

    log::info("TinexusServer initialized successfully on display '{}'", m_display_socket);
    return true;
}

void TinexusServer::run() {
    m_running = true;
    log::info("TinexusServer event loop running. Waiting for Wayland clients...");
}

void TinexusServer::stop() {
    m_running = false;
    log::info("TinexusServer stopping event loop...");
}

const std::string& TinexusServer::wayland_display() const noexcept {
    return m_display_socket;
}

} // namespace tinexus::comp
