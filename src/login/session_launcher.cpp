#include "login/session_launcher.hpp"
#include "common/logger.hpp"
#include <unistd.h>
#include <grp.h>
#include <pwd.h>
#include <cstdlib>

namespace tinexus::login {

bool SessionLauncher::launch_session(const std::string& username, uid_t uid, gid_t gid) {
    log::info("SessionLauncher: Preparing session launch for user '{}' (UID: {}, GID: {})...", username, uid, gid);

    // Populate required Wayland XDG desktop environment variables
    setenv("USER", username.c_str(), 1);
    setenv("LOGNAME", username.c_str(), 1);
    setenv("XDG_SESSION_TYPE", "wayland", 1);
    setenv("XDG_CURRENT_DESKTOP", "Tinexus", 1);
    setenv("WAYLAND_DISPLAY", "wayland-0", 1);

    log::info("SessionLauncher: Session environment populated successfully for user '{}'", username);
    return true;
}

} // namespace tinexus::login
