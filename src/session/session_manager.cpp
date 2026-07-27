#include "session/session_manager.hpp"
#include "common/logger.hpp"

namespace tinexus::session {

SessionManager& SessionManager::instance() noexcept {
    static SessionManager s_instance;
    return s_instance;
}

void SessionManager::transition_state(SessionState new_state) noexcept {
    log::info("SessionManager: Transitioning state {} -> {}", static_cast<int>(m_state), static_cast<int>(new_state));
    m_state = new_state;
}

bool SessionManager::start_session(bool dry_run) {
    log::info("SessionManager: Starting desktop session (dry_run={})", dry_run);
    transition_state(SessionState::Starting);
    transition_state(SessionState::Authenticating);
    transition_state(SessionState::Launching);
    transition_state(SessionState::Running);
    return true;
}

bool SessionManager::stop_session() {
    log::info("SessionManager: Stopping desktop session");
    transition_state(SessionState::Stopping);
    transition_state(SessionState::Stopped);
    return true;
}

void SessionManager::register_daemon(const std::string& name, pid_t pid) {
    m_daemons[name] = DaemonProc{name, pid, 0, true};
    log::info("SessionManager Supervisor: Registered daemon '{}' (PID={})", name, pid);
}

bool SessionManager::handle_daemon_crash(const std::string& name) {
    auto it = m_daemons.find(name);
    if (it == m_daemons.end()) return false;

    it->second.restart_count++;
    it->second.pid = 9000 + static_cast<pid_t>(it->second.restart_count); // Mock restarted PID
    log::warn("SessionManager Supervisor: Daemon '{}' crashed! Automatically restarted (attempt #{}, new PID={})",
              name, it->second.restart_count, it->second.pid);
    return true;
}

} // namespace tinexus::session
