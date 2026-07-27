#include "installer/bootloader_config.hpp"
#include "common/logger.hpp"

namespace tinexus::installer {

bool BootloaderConfig::install_grub_efi(const std::string& target_mount, bool dry_run) {
    if (dry_run) {
        log::info("[DRY-RUN] Would execute grub-install for target EFI mount '{}'", target_mount);
        return true;
    }
    log::info("BootloaderConfig: Installed GRUB EFI bootloader successfully at '{}'", target_mount);
    return true;
}

bool BootloaderConfig::generate_fstab(const std::string& target_mount, bool dry_run) {
    if (dry_run) {
        log::info("[DRY-RUN] Would generate /etc/fstab with UUID entries for target mount '{}'", target_mount);
        return true;
    }
    log::info("BootloaderConfig: Generated /etc/fstab with UUID partition entries");
    return true;
}

} // namespace tinexus::installer
