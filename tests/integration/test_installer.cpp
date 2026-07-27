#include <iostream>
#include <cassert>
#include "common/logger.hpp"
#include "installer/disk_inspector.hpp"
#include "installer/partition_engine.hpp"
#include "installer/squashfs_extractor.hpp"
#include "installer/bootloader_config.hpp"
#include "installer/install_controller.hpp"

void test_disk_inspector_and_validation() {
    tinexus::installer::DiskInspector inspector;
    auto disks = inspector.discover_disks();
    assert(!disks.empty());
    assert(inspector.validate_disk(disks[0]) == true);

    tinexus::installer::DiskInfo live_media;
    live_media.is_live_media = true;
    assert(inspector.validate_disk(live_media) == false); // Protected!
    std::cout << "[PASS] test_disk_inspector_and_validation\n";
}

void test_partition_engine_dry_run() {
    tinexus::installer::PartitionEngine engine;
    assert(engine.create_gpt_layout("/dev/nvme0n1", true));
    assert(engine.format_partition("/dev/nvme0n1p1", "vfat", true));
    assert(engine.format_partition("/dev/nvme0n1p2", "ext4", true));
    std::cout << "[PASS] test_partition_engine_dry_run\n";
}

void test_squashfs_extractor_and_bootloader() {
    assert(tinexus::installer::SquashfsExtractor::extract_rootfs("/live/rootfs.squashfs", "/mnt", true));
    assert(tinexus::installer::BootloaderConfig::install_grub_efi("/mnt", true));
    assert(tinexus::installer::BootloaderConfig::generate_fstab("/mnt", true));
    std::cout << "[PASS] test_squashfs_extractor_and_bootloader\n";
}

void test_install_controller_dry_run_flow() {
    tinexus::installer::DiskInspector inspector;
    auto disks = inspector.discover_disks();

    tinexus::installer::InstallController controller;
    bool success = controller.run_installation(disks[0], true);
    assert(success);
    assert(controller.state() == tinexus::installer::InstallState::Finished);
    std::cout << "[PASS] test_install_controller_dry_run_flow\n";
}

int main() {
    tinexus::log::set_component_name("integration_test_installer");
    tinexus::log::info("Running Integration Test Suite for Tinexus OS Installer...");

    test_disk_inspector_and_validation();
    test_partition_engine_dry_run();
    test_squashfs_extractor_and_bootloader();
    test_install_controller_dry_run_flow();

    tinexus::log::info("All Tinexus OS Installer integration tests passed 100%!");
    return 0;
}
