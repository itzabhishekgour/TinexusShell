#ifndef TINEXUS_SESSION_MANAGER_HPP
#define TINEXUS_SESSION_MANAGER_HPP

#include <string>
#include <cstdint>

namespace tinexus::session {

enum class SessionState : uint8_t {
    Booting = 0,
    EnvironmentReady = 1,
    ServicedStarted = 2,
    DesktopReady = 3,
    Running = 4,
    Stopping = 5,
    Stopped = 6
};

inline const char* session_state_to_string(SessionState state) noexcept {
    switch (state) {
        case SessionState::Booting: return "Booting";
        case SessionState::EnvironmentReady: return "EnvironmentReady";
        case SessionState::ServicedStarted: return "ServicedStarted";
        case SessionState::DesktopReady: return "DesktopReady";
        case SessionState::Running: return "Running";
        case SessionState::Stopping: return "Stopping";
        case SessionState::Stopped: return "Stopped";
        default: return "Unknown";
    }
}

class SessionManager {
public:
    static SessionManager& instance() noexcept;

    SessionManager() = default;
    ~SessionManager() = default;

    bool start_session(bool dry_run = false);
    bool stop_session();

    SessionState state() const noexcept { return m_state; }

private:
    SessionState m_state{SessionState::Booting};
};

} // namespace tinexus::session

#endif // TINEXUS_SESSION_MANAGER_HPP
