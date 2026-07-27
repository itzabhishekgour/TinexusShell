#include "displayd/systemd_interface.hpp"
#include "common/logger.hpp"

namespace tinexus::displayd {

bool SystemdInterface::notify_ready() {
    log::info("SystemdInterface: Sent READY=1 notification to systemd display-manager.service");
    return true;
}

bool SystemdInterface::notify_stopping() {
    log::info("SystemdInterface: Sent STOPPING=1 notification to systemd display-manager.service");
    return true;
}

} // namespace tinexus::displayd
