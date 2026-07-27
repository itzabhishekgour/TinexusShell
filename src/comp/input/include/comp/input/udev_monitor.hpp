#ifndef TINEXUS_COMP_INPUT_UDEV_MONITOR_HPP
#define TINEXUS_COMP_INPUT_UDEV_MONITOR_HPP

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace tinexus::comp {

enum class UdevEventType {
    Add,
    Remove,
    Change
};

enum class UdevDeviceSubsystem {
    Input,
    Drm,
    Unknown
};

struct UdevDeviceEvent {
    UdevEventType type{UdevEventType::Add};
    UdevDeviceSubsystem subsystem{UdevDeviceSubsystem::Input};
    std::string sysname;
    std::string devnode;
};

using UdevEventCallback = std::function<void(const UdevDeviceEvent&)>;

class UdevMonitor {
public:
    UdevMonitor() = default;
    ~UdevMonitor() = default;

    bool initialize();
    void set_event_callback(UdevEventCallback callback);
    void poll_events();

    [[nodiscard]] bool is_active() const noexcept { return m_active; }
    [[nodiscard]] size_t total_events_processed() const noexcept { return m_events_processed; }

    void simulate_hotplug_event(UdevEventType type, UdevDeviceSubsystem subsystem, const std::string& devnode);

private:
    bool m_active{false};
    size_t m_events_processed{0};
    UdevEventCallback m_callback;
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_INPUT_UDEV_MONITOR_HPP
