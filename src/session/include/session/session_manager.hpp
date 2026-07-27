#ifndef TINEXUS_SESSION_SESSION_MANAGER_HPP
#define TINEXUS_SESSION_SESSION_MANAGER_HPP

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace tinexus::session {

enum class SessionState {
    Stopped,
    Starting,
    Authenticating,
    Launching,
    Running,
    Stopping,
    Failed
};

struct DaemonProc {
    std::string name;
    pid_t pid{0};
    uint32_t restart_count{0};
    bool active{true};
};

class SessionManager {
public:
    static SessionManager& instance() noexcept;

    SessionManager() = default;
    ~SessionManager() = default;

    [[nodiscard]] SessionState state() const noexcept { return m_state; }
    void transition_state(SessionState new_state) noexcept;
    bool start_session(bool dry_run = false);
    bool stop_session();

    void register_daemon(const std::string& name, pid_t pid);
    bool handle_daemon_crash(const std::string& name);
    [[nodiscard]] const std::map<std::string, DaemonProc>& daemons() const noexcept { return m_daemons; }

private:
    SessionState m_state{SessionState::Stopped};
    std::map<std::string, DaemonProc> m_daemons;
};

} // namespace tinexus::session

#endif // TINEXUS_SESSION_SESSION_MANAGER_HPP
