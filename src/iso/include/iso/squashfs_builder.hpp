#ifndef TINEXUS_ISO_SQUASHFS_BUILDER_HPP
#define TINEXUS_ISO_SQUASHFS_BUILDER_HPP

#include <string>

namespace tinexus::iso {

class SquashfsBuilder {
public:
    static bool compress_rootfs(const std::string& rootfs_dir, const std::string& output_squashfs, bool dry_run = false);
};

} // namespace tinexus::iso

#endif // TINEXUS_ISO_SQUASHFS_BUILDER_HPP
