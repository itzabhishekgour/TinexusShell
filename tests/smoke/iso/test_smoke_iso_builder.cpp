#include <cassert>
#include <iostream>
#include "iso/iso_builder.hpp"
#include "iso/boot_config.hpp"

using namespace tinexus::iso;

int main() {
    std::cout << "[+] Running smoke_iso_builder test suite..." << std::endl;

    IsoBuilder builder;
    assert(builder.build_iso("/tmp/Tinexus-x86_64.iso", true)); // Dry-run ISO build pipeline

    // Verify systemd-boot & GRUB EFI config generation
    assert(BootConfig::generate_grub_config("/tmp/grub.cfg", true));
    assert(BootConfig::generate_efi_image("/tmp/efi.img", true));

    std::cout << "[+] smoke_iso_builder: ALL ASSERTIONS PASSED (100%)" << std::endl;
    return 0;
}
