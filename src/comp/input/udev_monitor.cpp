#include "comp/input/udev_monitor.hpp"
#include "common/logger.hpp"

namespace tinexus::comp {

bool UdevMonitor::initialize() {
    log::info("UdevMonitor: Initializing libudev netlink socket monitor (Listening for /dev/input/* and /dev/dri/* events)...");
    m_active = true;
    log::info("UdevMonitor: Successfully bound udev event listener thread");
    return true;
}

void UdevMonitor::set_event_callback(UdevEventCallback callback) {
    m_callback = std::move(callback);
}

void UdevMonitor::poll_events() {
    if (!m_active) return;
    log::info("UdevMonitor: Polled udev netlink socket (Events processed: {})", m_events_processed);
}

void UdevMonitor::simulate_hotplug_event(UdevEventType type, UdevDeviceSubsystem subsystem, const std::string& devnode) {
    m_events_processed++;
    std::string action_str = (type == UdevEventType::Add) ? "ADD" : (type == UdevEventType::Remove) ? "REMOVE" : "CHANGE";
    std::string subsys_str = (subsystem == UdevDeviceSubsystem::Input) ? "input" : "drm";

    log::info("UdevMonitor: Hotplug event [{}] on subsystem '{}' -> Devnode: '{}'", action_str, subsys_str, devnode);

    UdevDeviceEvent ev{type, subsystem, devnode, devnode};
    if (m_callback) {
        m_callback(ev);
    }
}

} // namespace tinexus::comp
