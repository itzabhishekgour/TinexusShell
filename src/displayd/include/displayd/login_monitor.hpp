#ifndef TINEXUS_DISPLAYD_LOGIN_MONITOR_HPP
#define TINEXUS_DISPLAYD_LOGIN_MONITOR_HPP

#include <sys/types.h>

namespace tinexus::displayd {

class LoginMonitor {
public:
    LoginMonitor() = default;
    ~LoginMonitor() = default;

    bool spawn_login_screen();
    bool handle_crash_and_restart();
    [[nodiscard]] pid_t login_pid() const noexcept { return m_login_pid; }

private:
    pid_t m_login_pid{-1};
};

} // namespace tinexus::displayd

#endif // TINEXUS_DISPLAYD_LOGIN_MONITOR_HPP
