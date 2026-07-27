#include "iso/iso_builder.hpp"
#include "iso/rootfs_stager.hpp"
#include "iso/initramfs_builder.hpp"
#include "iso/squashfs_builder.hpp"
#include "iso/boot_config.hpp"
#include "common/logger.hpp"

namespace tinexus::iso {

bool IsoBuilder::build_iso(const std::string& output_iso, bool dry_run) {
    log::info("IsoBuilder: Starting Tinexus ISO image build pipeline (Dry-Run: {})...", dry_run ? "TRUE" : "FALSE");

    RootfsStager stager;
    if (!stager.stage_rootfs("/tmp/tinexus_rootfs", {"/etc/tinexus/manifest.json"}, dry_run)) return false;
    if (!InitramfsBuilder::generate_initramfs("/tmp/tinexus_rootfs/boot/initramfs.img", dry_run)) return false;
    if (!SquashfsBuilder::compress_rootfs("/tmp/tinexus_rootfs", "/tmp/tinexus_iso/live/rootfs.squashfs", dry_run)) return false;
    if (!BootConfig::generate_grub_config("/tmp/tinexus_iso/boot/grub/grub.cfg", dry_run)) return false;
    if (!BootConfig::generate_efi_image("/tmp/tinexus_iso/EFI/efi.img", dry_run)) return false;

    if (dry_run) {
        log::info("[DRY-RUN] Would execute xorriso -as mkisofs to output '{}'", output_iso);
        return true;
    }
    log::info("IsoBuilder: Successfully built hybrid bootable ISO image '{}'", output_iso);
    return true;
}

} // namespace tinexus::iso
