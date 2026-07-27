#include "installer/install_controller.hpp"
#include "common/logger.hpp"

namespace tinexus::installer {

bool InstallController::run_installation(const DiskInfo& disk, bool dry_run) {
    log::info("InstallController: Starting Tinexus OS installation process (Dry-Run: {})...", dry_run ? "TRUE" : "FALSE");

    m_state = InstallState::ScanDisks;
    if (!m_inspector.validate_disk(disk)) {
        m_state = InstallState::Failed;
        return false;
    }

    m_state = InstallState::Confirm;
    log::info("InstallController: Confirmed installation safety gate on device '{}'", disk.device_path);

    m_state = InstallState::Partition;
    if (!m_partitioner.create_gpt_layout(disk.device_path, dry_run)) {
        m_state = InstallState::Failed;
        return false;
    }

    m_state = InstallState::Format;
    if (!m_partitioner.format_partition(disk.device_path + "p1", "vfat", dry_run) ||
        !m_partitioner.format_partition(disk.device_path + "p2", "ext4", dry_run)) {
        m_state = InstallState::Failed;
        return false;
    }

    m_state = InstallState::ExtractRootFS;
    if (!SquashfsExtractor::extract_rootfs("/live/rootfs.squashfs", "/mnt/target", dry_run)) {
        m_state = InstallState::Failed;
        return false;
    }

    m_state = InstallState::InstallBootloader;
    if (!BootloaderConfig::install_grub_efi("/mnt/target", dry_run)) {
        m_state = InstallState::Failed;
        return false;
    }

    m_state = InstallState::GenerateConfig;
    if (!BootloaderConfig::generate_fstab("/mnt/target", dry_run)) {
        m_state = InstallState::Failed;
        return false;
    }

    m_state = InstallState::Finished;
    log::info("InstallController: Tinexus OS installation completed successfully!");
    return true;
}

} // namespace tinexus::installer
