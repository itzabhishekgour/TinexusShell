#include "installer/install_controller.hpp"
#include "common/logger.hpp"
#include <vector>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <filesystem>
#include <cctype>

namespace tinexus::installer {

static bool run_cmd(const std::vector<std::string>& args) {
    if (args.empty()) return false;
    pid_t pid = fork();
    if (pid == 0) {
        std::vector<char*> c_args;
        for (const auto& a : args) c_args.push_back(const_cast<char*>(a.c_str()));
        c_args.push_back(nullptr);
        execvp(c_args[0], c_args.data());
        _exit(127);
    }
    if (pid < 0) return false;
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

bool InstallController::run_installation(const DiskInfo& disk, bool dry_run) {
    log::info("InstallController: Starting Tinexus OS installation process on '{}' (Dry-Run: {})...", 
              disk.device_path, dry_run ? "TRUE" : "FALSE");

    m_state = BackendInstallState::ScanDisks;
    if (!m_inspector.validate_disk(disk)) {
        m_state = BackendInstallState::Failed;
        return false;
    }

    m_state = BackendInstallState::Confirm;
    log::info("InstallController: Confirmed installation safety gate on device '{}'", disk.device_path);

    // Dynamic partition path construction (/dev/nvme0n1p1 vs /dev/vda1)
    std::string p1 = disk.device_path;
    std::string p2 = disk.device_path;
    if (!disk.device_path.empty() && std::isdigit(disk.device_path.back())) {
        p1 += "p1";
        p2 += "p2";
    } else {
        p1 += "1";
        p2 += "2";
    }

    m_state = BackendInstallState::Partition;
    if (!m_partitioner.create_gpt_layout(disk.device_path, dry_run)) {
        m_state = BackendInstallState::Failed;
        return false;
    }

    m_state = BackendInstallState::Format;
    if (!m_partitioner.format_partition(p1, "vfat", dry_run) ||
        !m_partitioner.format_partition(p2, "ext4", dry_run)) {
        m_state = BackendInstallState::Failed;
        return false;
    }

    std::string target_mount = "/mnt/target";
    if (!dry_run) {
        std::filesystem::create_directories(target_mount);
        run_cmd({"mount", p2, target_mount});
        std::filesystem::create_directories(target_mount + "/boot/efi");
        run_cmd({"mount", p1, target_mount + "/boot/efi"});
    }

    m_state = BackendInstallState::ExtractRootFS;
    if (!SquashfsExtractor::extract_rootfs("/live/rootfs.squashfs", target_mount, dry_run)) {
        m_state = BackendInstallState::Failed;
        if (!dry_run) {
            run_cmd({"umount", "-R", target_mount});
        }
        return false;
    }

    m_state = BackendInstallState::GenerateConfig;
    if (!BootloaderConfig::generate_fstab(target_mount, dry_run)) {
        m_state = BackendInstallState::Failed;
        if (!dry_run) {
            run_cmd({"umount", "-R", target_mount});
        }
        return false;
    }

    m_state = BackendInstallState::InstallBootloader;
    if (!BootloaderConfig::install_grub_efi(target_mount, dry_run)) {
        m_state = BackendInstallState::Failed;
        if (!dry_run) {
            run_cmd({"umount", "-R", target_mount});
        }
        return false;
    }

    if (!dry_run) {
        run_cmd({"umount", "-R", target_mount});
    }

    m_state = BackendInstallState::Finished;
    log::info("InstallController: Tinexus OS installation completed successfully on '{}'!", disk.device_path);
    return true;
}

} // namespace tinexus::installer
