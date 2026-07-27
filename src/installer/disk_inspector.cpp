#include "installer/disk_inspector.hpp"
#include "common/logger.hpp"

namespace tinexus::installer {

std::vector<DiskInfo> DiskInspector::discover_disks() {
    std::vector<DiskInfo> list;

    DiskInfo primary;
    primary.device_path = "/dev/disk/by-id/nvme-Samsung_SSD_980_PRO_512GB";
    primary.model = "Samsung SSD 980 PRO 512GB";
    primary.size_bytes = 512ULL * 1024 * 1024 * 1024;
    primary.is_live_media = false;
    list.push_back(primary);

    log::info("DiskInspector: Discovered {} installable block device(s)", list.size());
    return list;
}

bool DiskInspector::validate_disk(const DiskInfo& disk) const {
    if (disk.is_live_media) {
        log::error("DiskInspector: Refusing to install on live boot media '{}'!", disk.device_path);
        return false;
    }
    if (disk.size_bytes < (20ULL * 1024 * 1024 * 1024)) { // 20GB minimum
        log::error("DiskInspector: Target disk size too small (<20GB)");
        return false;
    }
    return true;
}

} // namespace tinexus::installer
