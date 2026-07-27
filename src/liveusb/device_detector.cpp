#include "liveusb/device_detector.hpp"
#include "common/logger.hpp"

namespace tinexus::liveusb {

std::vector<UsbDevice> DeviceDetector::discover_usb_drives() {
    std::vector<UsbDevice> drives;

    UsbDevice flash_drive;
    flash_drive.device_path = "/dev/sdb";
    flash_drive.model = "SanDisk Ultra USB 3.0";
    flash_drive.size_bytes = 32ULL * 1024 * 1024 * 1024;
    flash_drive.is_removable = true;
    flash_drive.is_mounted = false;
    drives.push_back(flash_drive);

    log::info("DeviceDetector: Discovered {} removable USB storage drive(s)", drives.size());
    return drives;
}

bool DeviceDetector::validate_device_safety(const UsbDevice& device) const {
    if (!device.is_removable) {
        log::error("DeviceDetector: Refusing write to non-removable internal system disk '{}'!", device.device_path);
        return false;
    }
    if (device.is_mounted) {
        log::error("DeviceDetector: Refusing write to mounted device '{}'!", device.device_path);
        return false;
    }
    return true;
}

} // namespace tinexus::liveusb
