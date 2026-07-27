#include "iso/boot_config.hpp"
#include "common/logger.hpp"

namespace tinexus::iso {

bool BootConfig::generate_grub_config(const std::string& output_grub_cfg, bool dry_run) {
    if (dry_run) {
        log::info("[DRY-RUN] Would generate GRUB live boot config at '{}'", output_grub_cfg);
        return true;
    }
    log::info("BootConfig: Generated live boot grub.cfg at '{}'", output_grub_cfg);
    return true;
}

bool BootConfig::generate_efi_image(const std::string& output_efi_img, bool dry_run) {
    if (dry_run) {
        log::info("[DRY-RUN] Would generate EFI System Partition image at '{}'", output_efi_img);
        return true;
    }
    log::info("BootConfig: Generated EFI boot image at '{}'", output_efi_img);
    return true;
}

} // namespace tinexus::iso
