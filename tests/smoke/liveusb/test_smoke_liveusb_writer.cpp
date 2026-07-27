#include <cassert>
#include <iostream>
#include "liveusb/liveusb_controller.hpp"
#include "liveusb/device_detector.hpp"

using namespace tinexus::liveusb;

int main() {
    std::cout << "[+] Running smoke_liveusb_writer test suite..." << std::endl;

    UsbDevice dev{"/dev/sdb", "SanDisk Ultra", 32000000000ULL, true, false};
    DeviceDetector detector;
    assert(detector.validate_device_safety(dev));

    // Verify system disk overwrite protection (Non-removable device rejected)
    UsbDevice sys_disk{"/dev/nvme0n1", "Internal NVMe", 512000000000ULL, false, false};
    assert(!detector.validate_device_safety(sys_disk));

    LiveUsbController controller;
    assert(controller.run_liveusb_creation("/tmp/Tinexus-x86_64.iso", dev, true)); // Dry-run USB write
    assert(controller.state() == LiveUsbState::Complete);

    std::cout << "[+] smoke_liveusb_writer: ALL ASSERTIONS PASSED (100%)" << std::endl;
    return 0;
}
