#ifndef TINEXUS_DISPLAYD_SESSION_LAUNCHER_HPP
#define TINEXUS_DISPLAYD_SESSION_LAUNCHER_HPP

#include <string>

namespace tinexus::displayd {

class SessionLauncher {
public:
    static bool launch_user_session(const std::string& username);
    static bool handle_logout();
};

} // namespace tinexus::displayd

#endif // TINEXUS_DISPLAYD_SESSION_LAUNCHER_HPP
