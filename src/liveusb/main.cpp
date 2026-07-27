#include "liveusb/liveusb_controller.hpp"
#include "tinexus/client.hpp"
#include "common/logger.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    tinexus::log::set_component_name("tinexus-liveusb");
    tinexus::log::info("Starting Tinexus Live USB Boot Utility (tinexus-liveusb)...");

    tinexus::Client sdk_client;
    if (sdk_client.connect().is_ok()) {
        tinexus::log::info("Tinexus Live USB: Connected to Tinexus Platform IPC broker via SDK.");
    }

    bool dry_run = true;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--dry-run") dry_run = true;
    }

    tinexus::liveusb::DeviceDetector detector;
    auto drives = detector.discover_usb_drives();
    if (!drives.empty()) {
        tinexus::liveusb::LiveUsbController controller;
        controller.run_liveusb_creation("Tinexus-0.1.0-alpha.iso", drives[0], dry_run);
    }

    sdk_client.disconnect();
    return 0;
}
