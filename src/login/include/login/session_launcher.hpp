#ifndef TINEXUS_LOGIN_SESSION_LAUNCHER_HPP
#define TINEXUS_LOGIN_SESSION_LAUNCHER_HPP

#include <string>
#include <sys/types.h>

namespace tinexus::login {

class SessionLauncher {
public:
    static bool launch_session(const std::string& username, uid_t uid, gid_t gid);
};

} // namespace tinexus::login

#endif // TINEXUS_LOGIN_SESSION_LAUNCHER_HPP
