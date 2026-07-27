#include <cassert>
#include <iostream>
#include "comp/input/udev_monitor.hpp"

using namespace tinexus::comp;

int main() {
    std::cout << "[+] Running smoke_udev_hotplug test suite..." << std::endl;

    UdevMonitor monitor;
    assert(monitor.initialize() == true);
    assert(monitor.is_active() == true);

    bool input_added = false;
    bool drm_added = false;

    monitor.set_event_callback([&](const UdevDeviceEvent& ev) {
        if (ev.subsystem == UdevDeviceSubsystem::Input && ev.type == UdevEventType::Add) {
            input_added = true;
        } else if (ev.subsystem == UdevDeviceSubsystem::Drm && ev.type == UdevEventType::Add) {
            drm_added = true;
        }
    });

    // Simulate input device hotplug
    monitor.simulate_hotplug_event(UdevEventType::Add, UdevDeviceSubsystem::Input, "/dev/input/event4");
    assert(input_added == true);

    // Simulate DRM display monitor hotplug
    monitor.simulate_hotplug_event(UdevEventType::Add, UdevDeviceSubsystem::Drm, "/dev/dri/card0");
    assert(drm_added == true);

    assert(monitor.total_events_processed() == 2);
    monitor.poll_events();

    std::cout << "[+] smoke_udev_hotplug: ALL ASSERTIONS PASSED (100%)" << std::endl;
    return 0;
}
