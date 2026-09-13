#include "installer/squashfs_extractor.hpp"
#include "common/logger.hpp"
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>
#include <filesystem>

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

bool SquashfsExtractor::extract_rootfs(const std::string& image_path, const std::string& target_mount, bool dry_run) {
    if (dry_run) {
        log::info("[DRY-RUN] Would extract SquashFS rootfs '{}' to '{}'", image_path, target_mount);
        return true;
    }
    log::info("SquashfsExtractor: Extracting rootfs from '{}' to '{}'...", image_path, target_mount);

    std::string real_img = image_path;

    // 1. Check if squashfs mount is dynamically listed in /proc/mounts
    {
        std::ifstream mounts("/proc/mounts");
        std::string line;
        while (std::getline(mounts, line)) {
            if (line.find("squashfs") != std::string::npos) {
                std::istringstream iss(line);
                std::string dev, mnt, type;
                if (iss >> dev >> mnt >> type && type == "squashfs") {
                    if (std::filesystem::exists(mnt)) {
                        real_img = mnt;
                        log::info("SquashfsExtractor: Dynamically located active squashfs mount at '{}'", real_img);
                        break;
                    }
                }
            }
        }
    }

    if (!std::filesystem::exists(real_img)) {
        // Search alternative locations on live ISO
        for (const auto& cand : {"/rofs", "/live/rootfs.squashfs", "/live/live/rootfs.squashfs", "/mnt/live/rootfs.squashfs"}) {
            if (std::filesystem::exists(cand)) {
                real_img = cand;
                log::info("SquashfsExtractor: Found alternative rootfs candidate at '{}'", real_img);
                break;
            }
        }
    }

    bool ok = false;
    if (real_img == "/rofs" || (std::filesystem::exists(real_img) && std::filesystem::is_directory(real_img))) {
        log::info("SquashfsExtractor: Copying directly from mounted directory '{}'...", real_img);
        ok = run_cmd({"cp", "-a", real_img + "/.", target_mount + "/"});
    } else {
        log::info("SquashfsExtractor: Executing unsquashfs -f -d '{}' '{}'...", target_mount, real_img);
        ok = run_cmd({"unsquashfs", "-f", "-d", target_mount, real_img});
    }

    if (!ok) {
        log::error("SquashfsExtractor: Failed to extract rootfs to '{}'", target_mount);
        return false;
    }

    log::info("SquashfsExtractor: Successfully extracted SquashFS rootfs to '{}'", target_mount);
    return true;
}

} // namespace tinexus::installer
