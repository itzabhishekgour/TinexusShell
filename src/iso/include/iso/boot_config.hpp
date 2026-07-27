#ifndef TINEXUS_ISO_BOOT_CONFIG_HPP
#define TINEXUS_ISO_BOOT_CONFIG_HPP

#include <string>

namespace tinexus::iso {

class BootConfig {
public:
    static bool generate_grub_config(const std::string& output_grub_cfg, bool dry_run = false);
    static bool generate_efi_image(const std::string& output_efi_img, bool dry_run = false);
};

} // namespace tinexus::iso

#endif // TINEXUS_ISO_BOOT_CONFIG_HPP
