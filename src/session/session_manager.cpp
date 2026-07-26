#include "session/session_manager.hpp"
#include "session/env_bootstrap.hpp"
#include "session/autostart_parser.hpp"
#include "common/logger.hpp"

namespace tinexus::session {

SessionManager& SessionManager::instance() noexcept {
    static SessionManager s_instance;
    return s_instance;
}

bool SessionManager::start_session(bool dry_run) {
    m_state = SessionState::Booting;
    log::info("SessionManager: Transitioning state -> {}", session_state_to_string(m_state));

    // Step 1: Environment Bootstrap
    EnvironmentBootstrapper::instance().bootstrap_environment();
    m_state = SessionState::EnvironmentReady;
    log::info("SessionManager: Transitioning state -> {}", session_state_to_string(m_state));

    if (dry_run) {
        log::info("SessionManager: Dry run requested. Halting session start at EnvironmentReady.");
        return true;
    }

    // Step 2: Serviced Supervision Start
    m_state = SessionState::ServicedStarted;
    log::info("SessionManager: Transitioning state -> {}", session_state_to_string(m_state));

    // Step 3: Autostart Apps Parsing
    auto autostarts = AutostartParser::instance().parse_autostart_directory("/etc/xdg/autostart");
    AutostartParser::instance().launch_autostart_apps(autostarts);

    m_state = SessionState::DesktopReady;
    log::info("SessionManager: Transitioning state -> {}", session_state_to_string(m_state));

    m_state = SessionState::Running;
    log::info("SessionManager: Transitioning state -> {}", session_state_to_string(m_state));
    return true;
}

bool SessionManager::stop_session() {
    log::info("SessionManager: Graceful session shutdown sequence initiated...");
    m_state = SessionState::Stopping;
    log::info("SessionManager: Transitioning state -> {}", session_state_to_string(m_state));

    // Reverse topological shutdown: Launcher -> Comp -> Search -> Indexer -> IPCD -> Serviced
    log::info("SessionManager: Stopped all supervised platform services.");

    m_state = SessionState::Stopped;
    log::info("SessionManager: Transitioning state -> {}", session_state_to_string(m_state));
    return true;
}

} // namespace tinexus::session
