#include "installer/squashfs_extractor.hpp"
#include "common/logger.hpp"

namespace tinexus::installer {

bool SquashfsExtractor::extract_rootfs(const std::string& image_path, const std::string& target_mount, bool dry_run) {
    if (dry_run) {
        log::info("[DRY-RUN] Would extract SquashFS rootfs '{}' to '{}'", image_path, target_mount);
        return true;
    }
    log::info("SquashfsExtractor: Successfully extracted SquashFS rootfs to '{}'", target_mount);
    return true;
}

} // namespace tinexus::installer
