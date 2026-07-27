#ifndef TINEXUS_INSTALLER_BOOTLOADER_CONFIG_HPP
#define TINEXUS_INSTALLER_BOOTLOADER_CONFIG_HPP

#include <string>

namespace tinexus::installer {

class BootloaderConfig {
public:
    static bool install_grub_efi(const std::string& target_mount, bool dry_run = false);
    static bool generate_fstab(const std::string& target_mount, bool dry_run = false);
};

} // namespace tinexus::installer

#endif // TINEXUS_INSTALLER_BOOTLOADER_CONFIG_HPP
