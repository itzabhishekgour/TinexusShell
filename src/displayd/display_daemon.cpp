#include "displayd/display_daemon.hpp"
#include "common/logger.hpp"

namespace tinexus::displayd {

bool DisplayDaemon::bootstrap() {
    log::info("DisplayDaemon: Bootstrapping Tinexus Display Manager...");

    if (!m_vt.allocate_vt(7)) return false;
    if (!m_seat.acquire_seat("seat0")) return false;
    if (!m_login.spawn_login_screen()) return false;

    SystemdInterface::notify_ready();
    log::info("DisplayDaemon: Display Manager operational and ready for graphical login.");
    return true;
}

bool DisplayDaemon::shutdown() {
    log::info("DisplayDaemon: Shutting down Display Manager...");
    SystemdInterface::notify_stopping();
    m_seat.release_seat();
    return true;
}

} // namespace tinexus::displayd
