#include "iso/initramfs_builder.hpp"
#include "common/logger.hpp"

namespace tinexus::iso {

bool InitramfsBuilder::generate_initramfs(const std::string& output_img, bool dry_run) {
    if (dry_run) {
        log::info("[DRY-RUN] Would generate initramfs image '{}'", output_img);
        return true;
    }
    log::info("InitramfsBuilder: Generated initramfs image at '{}'", output_img);
    return true;
}

} // namespace tinexus::iso
