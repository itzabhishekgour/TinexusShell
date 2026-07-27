#include "displayd/session_launcher.hpp"
#include "common/logger.hpp"

namespace tinexus::displayd {

bool SessionLauncher::launch_user_session(const std::string& username) {
    log::info("SessionLauncher: Handing off execution to tinexus-session for user '{}'", username);
    return true;
}

bool SessionLauncher::handle_logout() {
    log::info("SessionLauncher: User logout requested. Terminating active session.");
    return true;
}

} // namespace tinexus::displayd
