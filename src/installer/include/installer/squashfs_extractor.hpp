#ifndef TINEXUS_INSTALLER_SQUASHFS_EXTRACTOR_HPP
#define TINEXUS_INSTALLER_SQUASHFS_EXTRACTOR_HPP

#include <string>

namespace tinexus::installer {

class SquashfsExtractor {
public:
    static bool extract_rootfs(const std::string& image_path, const std::string& target_mount, bool dry_run = false);
};

} // namespace tinexus::installer

#endif // TINEXUS_INSTALLER_SQUASHFS_EXTRACTOR_HPP
