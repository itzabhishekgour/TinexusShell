#include <cassert>
#include <iostream>
#include "installer/install_controller.hpp"
#include "installer/partition_engine.hpp"

using namespace tinexus::installer;

int main() {
    std::cout << "[+] Running smoke_installer_partition test suite..." << std::endl;

    DiskInfo disk{"/dev/sda", "NVMe SSD", 256000000000ULL, false};
    PartitionEngine partitioner;

    // Verify GPT Partition Layout Creation
    assert(partitioner.create_gpt_layout(disk.device_path, true));
    assert(partitioner.format_partition("/dev/sdap1", "vfat", true));
    assert(partitioner.format_partition("/dev/sdap2", "ext4", true));

    InstallController controller;
    assert(controller.run_installation(disk, true)); // Dry-run OS installation
    assert(controller.state() == InstallState::Finished);

    std::cout << "[+] smoke_installer_partition: ALL ASSERTIONS PASSED (100%)" << std::endl;
    return 0;
}
