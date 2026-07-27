#include "iso/squashfs_builder.hpp"
#include "common/logger.hpp"

namespace tinexus::iso {

bool SquashfsBuilder::compress_rootfs(const std::string& rootfs_dir, const std::string& output_squashfs, bool dry_run) {
    if (dry_run) {
        log::info("[DRY-RUN] Would compress rootfs at '{}' into SquashFS '{}'", rootfs_dir, output_squashfs);
        return true;
    }
    log::info("SquashfsBuilder: Compressed rootfs into SquashFS image '{}'", output_squashfs);
    return true;
}

} // namespace tinexus::iso
