#ifndef TINEXUS_LIVEUSB_DEVICE_DETECTOR_HPP
#define TINEXUS_LIVEUSB_DEVICE_DETECTOR_HPP

#include <string>
#include <vector>
#include <cstdint>

namespace tinexus::liveusb {

struct UsbDevice {
    std::string device_path;
    std::string model;
    uint64_t size_bytes{0};
    bool is_removable{true};
    bool is_mounted{false};
};

class DeviceDetector {
public:
    DeviceDetector() = default;
    ~DeviceDetector() = default;

    std::vector<UsbDevice> discover_usb_drives();
    [[nodiscard]] bool validate_device_safety(const UsbDevice& device) const;
};

} // namespace tinexus::liveusb

#endif // TINEXUS_LIVEUSB_DEVICE_DETECTOR_HPP
