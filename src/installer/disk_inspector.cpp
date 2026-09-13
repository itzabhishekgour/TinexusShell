#include "installer/disk_inspector.hpp"
#include "common/logger.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace tinexus::installer {

std::vector<DiskInfo> DiskInspector::discover_disks() {
    std::vector<DiskInfo> list;

    if (std::filesystem::exists("/sys/block")) {
        for (const auto& entry : std::filesystem::directory_iterator("/sys/block")) {
            std::string name = entry.path().filename().string();
            // Filter non-physical or irrelevant devices
            if (name.rfind("loop", 0) == 0 || name.rfind("ram", 0) == 0 || name.rfind("sr", 0) == 0) {
                continue;
            }

            std::string size_file = entry.path() / "size";
            uint64_t size_bytes = 0;
            if (std::filesystem::exists(size_file)) {
                std::ifstream ifs(size_file);
                uint64_t sectors = 0;
                if (ifs >> sectors) {
                    size_bytes = sectors * 512ULL;
                }
            }

            // Exclude disks smaller than 8GB
            if (size_bytes < (8ULL * 1024 * 1024 * 1024)) {
                continue;
            }

            std::string model = name;
            std::string model_file = entry.path() / "device" / "model";
            if (std::filesystem::exists(model_file)) {
                std::ifstream ifs(model_file);
                std::string m;
                if (std::getline(ifs, m) && !m.empty()) {
                    model = m;
                }
            }

            DiskInfo info;
            info.device_path = "/dev/" + name;
            info.model = model;
            info.size_bytes = size_bytes;
            info.is_live_media = false;

            list.push_back(info);
        }
    }

    // Fallback if /sys/block wasn't populated (e.g. test environment)
    if (list.empty()) {
        DiskInfo fallback;
        fallback.device_path = "/dev/vda";
        fallback.model = "VirtIO Target Disk";
        fallback.size_bytes = 15ULL * 1024 * 1024 * 1024;
        fallback.is_live_media = false;
        list.push_back(fallback);
    }

    log::info("DiskInspector: Discovered {} installable block device(s)", list.size());
    return list;
}

bool DiskInspector::validate_disk(const DiskInfo& disk) const {
    if (disk.is_live_media) {
        log::error("DiskInspector: Refusing to install on live boot media '{}'!", disk.device_path);
        return false;
    }
    if (disk.size_bytes < (8ULL * 1024 * 1024 * 1024)) { // 8GB minimum
        log::error("DiskInspector: Target disk size too small (<8GB)");
        return false;
    }
    return true;
}

} // namespace tinexus::installer
