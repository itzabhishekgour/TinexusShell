#include "installer/partition_engine.hpp"
#include "common/logger.hpp"
#include <vector>
#include <string>
#include <unistd.h>
#include <sys/wait.h>

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

bool PartitionEngine::create_gpt_layout(const std::string& disk_path, bool dry_run) {
    if (dry_run) {
        log::info("[DRY-RUN] Would create GPT partition table on '{}'", disk_path);
        return true;
    }
    log::info("PartitionEngine: Creating GPT partition table on '{}'...", disk_path);

    // 1. Wipe existing partition signatures
    run_cmd({"wipefs", "-a", disk_path});

    // 2. Run parted: 512MB EFI partition (p1), remainder ext4 root (p2)
    bool ok = run_cmd({
        "parted", "-s", disk_path,
        "mklabel", "gpt",
        "mkpart", "ESP", "fat32", "1MiB", "513MiB",
        "set", "1", "esp", "on",
        "mkpart", "TinexusRoot", "ext4", "513MiB", "100%"
    });

    if (!ok) {
        log::error("PartitionEngine: parted command failed on '{}'", disk_path);
        return false;
    }

    run_cmd({"udevadm", "settle", "--timeout=10"});
    log::info("PartitionEngine: Successfully created GPT partition table on '{}'", disk_path);
    return true;
}

bool PartitionEngine::format_partition(const std::string& partition_path, const std::string& fs_type, bool dry_run) {
    if (dry_run) {
        log::info("[DRY-RUN] Would format partition '{}' as '{}'", partition_path, fs_type);
        return true;
    }
    log::info("PartitionEngine: Formatting partition '{}' as '{}'...", partition_path, fs_type);
    bool ok = false;
    if (fs_type == "vfat") {
        ok = run_cmd({"mkfs.vfat", "-F", "32", "-n", "TINEXUS_EFI", partition_path});
    } else if (fs_type == "ext4") {
        ok = run_cmd({"mkfs.ext4", "-F", "-L", "TINEXUS_ROOT", partition_path});
    } else {
        log::error("PartitionEngine: Unsupported filesystem type '{}'", fs_type);
        return false;
    }

    if (!ok) {
        log::error("PartitionEngine: Failed to format partition '{}' as '{}'", partition_path, fs_type);
        return false;
    }
    log::info("PartitionEngine: Successfully formatted '{}' as '{}'", partition_path, fs_type);
    return true;
}

} // namespace tinexus::installer
