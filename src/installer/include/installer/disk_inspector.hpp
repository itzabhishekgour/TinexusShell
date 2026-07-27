#ifndef TINEXUS_INSTALLER_DISK_INSPECTOR_HPP
#define TINEXUS_INSTALLER_DISK_INSPECTOR_HPP

#include <string>
#include <vector>
#include <cstdint>

namespace tinexus::installer {

struct DiskInfo {
    std::string device_path;
    std::string model;
    uint64_t size_bytes{0};
    bool is_live_media{false};
};

class DiskInspector {
public:
    DiskInspector() = default;
    ~DiskInspector() = default;

    std::vector<DiskInfo> discover_disks();
    [[nodiscard]] bool validate_disk(const DiskInfo& disk) const;
};

} // namespace tinexus::installer

#endif // TINEXUS_INSTALLER_DISK_INSPECTOR_HPP
