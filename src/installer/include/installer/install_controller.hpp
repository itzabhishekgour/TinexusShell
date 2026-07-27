#ifndef TINEXUS_INSTALLER_INSTALL_CONTROLLER_HPP
#define TINEXUS_INSTALLER_INSTALL_CONTROLLER_HPP

#include "installer/disk_inspector.hpp"
#include "installer/partition_engine.hpp"
#include "installer/squashfs_extractor.hpp"
#include "installer/bootloader_config.hpp"

namespace tinexus::installer {

enum class InstallState {
    Idle,
    ScanDisks,
    SelectDisk,
    Confirm,
    Partition,
    Format,
    ExtractRootFS,
    InstallBootloader,
    GenerateConfig,
    Finished,
    Failed
};

class InstallController {
public:
    InstallController() = default;
    ~InstallController() = default;

    bool run_installation(const DiskInfo& disk, bool dry_run = false);
    [[nodiscard]] InstallState state() const noexcept { return m_state; }

private:
    InstallState m_state{InstallState::Idle};
    DiskInspector m_inspector;
    PartitionEngine m_partitioner;
};

} // namespace tinexus::installer

#endif // TINEXUS_INSTALLER_INSTALL_CONTROLLER_HPP
