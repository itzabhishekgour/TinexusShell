#include <iostream>
#include <cassert>
#include "common/logger.hpp"
#include "liveusb/device_detector.hpp"
#include "liveusb/iso_verifier.hpp"
#include "liveusb/raw_writer.hpp"
#include "liveusb/verification.hpp"
#include "liveusb/progress_tracker.hpp"
#include "liveusb/persistence_mgr.hpp"
#include "liveusb/liveusb_controller.hpp"

void test_device_detector_and_safety_gates() {
    tinexus::liveusb::DeviceDetector detector;
    auto drives = detector.discover_usb_drives();
    assert(!drives.empty());
    assert(detector.validate_device_safety(drives[0]) == true);

    tinexus::liveusb::UsbDevice system_disk;
    system_disk.is_removable = false; // Protected!
    assert(detector.validate_device_safety(system_disk) == false);
    std::cout << "[PASS] test_device_detector_and_safety_gates\n";
}

void test_iso_verifier_and_raw_writer() {
    assert(tinexus::liveusb::IsoVerifier::verify_iso_integrity("Tinexus-0.1.0-alpha.iso"));
    assert(tinexus::liveusb::RawWriter::write_image("Tinexus-0.1.0-alpha.iso", "/dev/sdb", true));
    assert(tinexus::liveusb::VerificationEngine::verify_readback("Tinexus-0.1.0-alpha.iso", "/dev/sdb", true));
    std::cout << "[PASS] test_iso_verifier_and_raw_writer\n";
}

void test_progress_tracker_and_persistence() {
    tinexus::liveusb::ProgressTracker tracker;
    tracker.update_progress(16ULL * 1024 * 1024 * 1024, 32ULL * 1024 * 1024 * 1024);
    assert(tracker.percentage() == 50.0);
    assert(tinexus::liveusb::PersistenceManager::create_persistence_volume("/dev/sdb", 4096, true));
    std::cout << "[PASS] test_progress_tracker_and_persistence\n";
}

void test_liveusb_controller_10_stage_flow() {
    tinexus::liveusb::DeviceDetector detector;
    auto drives = detector.discover_usb_drives();

    tinexus::liveusb::LiveUsbController controller;
    bool success = controller.run_liveusb_creation("Tinexus-0.1.0-alpha.iso", drives[0], true);
    assert(success);
    assert(controller.state() == tinexus::liveusb::LiveUsbState::Complete);
    std::cout << "[PASS] test_liveusb_controller_10_stage_flow\n";
}

int main() {
    tinexus::log::set_component_name("integration_test_liveusb");
    tinexus::log::info("Running Integration Test Suite for Tinexus Live USB Boot Engine...");

    test_device_detector_and_safety_gates();
    test_iso_verifier_and_raw_writer();
    test_progress_tracker_and_persistence();
    test_liveusb_controller_10_stage_flow();

    tinexus::log::info("All Tinexus Live USB integration tests passed 100%!");
    return 0;
}
