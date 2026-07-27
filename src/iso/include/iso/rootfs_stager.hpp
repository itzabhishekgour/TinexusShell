#ifndef TINEXUS_ISO_ROOTFS_STAGER_HPP
#define TINEXUS_ISO_ROOTFS_STAGER_HPP

#include <string>
#include <vector>

namespace tinexus::iso {

class RootfsStager {
public:
    RootfsStager() = default;
    ~RootfsStager() = default;

    bool stage_rootfs(const std::string& target_dir, const std::vector<std::string>& manifests, bool dry_run = false);
};

} // namespace tinexus::iso

#endif // TINEXUS_ISO_ROOTFS_STAGER_HPP
