#ifndef TINEXUS_ISO_INITRAMFS_BUILDER_HPP
#define TINEXUS_ISO_INITRAMFS_BUILDER_HPP

#include <string>

namespace tinexus::iso {

class InitramfsBuilder {
public:
    static bool generate_initramfs(const std::string& output_img, bool dry_run = false);
};

} // namespace tinexus::iso

#endif // TINEXUS_ISO_INITRAMFS_BUILDER_HPP
