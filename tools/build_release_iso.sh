#!/usr/bin/env bash
# ==============================================================================
# Tinexus Platform — Real Hybrid Bootable ISO Builder (Production Grade)
#
# REQUIREMENTS before running:
#   1. build/kernel/vmlinuz      — Tinexus kernel image
#   2. build/kernel/initramfs.img — Tinexus initramfs (optional; built here if absent)
#   3. Host packages: xorriso squashfs-tools grub-efi-amd64-bin grub-pc-bin dosfstools mtools cpio gzip
#
# USAGE: bash build_release_iso.sh [--smoke-test]
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"
KERNEL_DIR="$BUILD_DIR/kernel"
OUTPUT_ISO="$BUILD_DIR/Tinexus-x86_64.iso"
ROOTFS_DIR="/tmp/tinexus_rootfs"
ISO_TREE="/tmp/tinexus_iso_tree"
WORK_DIR="/tmp/tinexus_iso_work"

SMOKE_TEST=0
if [[ "${1:-}" == "--smoke-test" ]]; then
    SMOKE_TEST=1
fi

# ── Logging ───────────────────────────────────────────────────────────────────
info()    { echo -e "\e[1;34m[INFO]\e[0m $1"; }
success() { echo -e "\e[1;32m[SUCCESS]\e[0m $1"; }
warn()    { echo -e "\e[1;33m[WARN]\e[0m $1"; }
error()   { echo -e "\e[1;31m[ERROR]\e[0m $1"; }
fatal()   { echo -e "\e[1;31m[FATAL]\e[0m $1"; exit 1; }

# ── Cleanup ───────────────────────────────────────────────────────────────────
cleanup() {
    rm -rf "$WORK_DIR"
}
trap cleanup EXIT

# ── Environment Verification ──────────────────────────────────────────────────
verify_env() {
    info "Verifying host toolchain..."
    local missing=()
    for tool in xorriso mksquashfs grub-mkimage mkfs.vfat mcopy mmd cpio gzip sha256sum file; do
        if ! command -v "$tool" &>/dev/null; then missing+=("$tool"); fi
    done
    if [ ${#missing[@]} -ne 0 ]; then
        fatal "Missing tools: ${missing[*]}\nInstall: sudo apt install xorriso squashfs-tools grub-efi-amd64-bin grub-pc-bin dosfstools mtools"
    fi

    export GRUB_EFI_MODS="/usr/lib/grub/x86_64-efi"
    export GRUB_BIOS_MODS="/usr/lib/grub/i386-pc"
    [ -d "$GRUB_EFI_MODS" ] || fatal "GRUB EFI module directory missing: $GRUB_EFI_MODS"
    [ -d "$GRUB_BIOS_MODS" ] || fatal "GRUB BIOS module directory missing: $GRUB_BIOS_MODS"

    export VMLINUZ="$KERNEL_DIR/vmlinuz"
    [ -f "$VMLINUZ" ] || fatal "Tinexus kernel not found at: $VMLINUZ"
    if file -b "$VMLINUZ" | grep -qi "ASCII\|text"; then
        fatal "$VMLINUZ appears to be a text file, not a real kernel."
    fi

    success "Toolchain & Environment OK."
}

# ── RootFS Generation ─────────────────────────────────────────────────────────
build_rootfs() {
    info "Building Tinexus root filesystem..."
    rm -rf "$ROOTFS_DIR" "$ISO_TREE" "$WORK_DIR"
    mkdir -p "$ROOTFS_DIR"/{bin,sbin,lib,lib64,usr/{bin,sbin,lib},etc,var/{log,run},run,dev,proc,sys,tmp,mnt,newroot,live,root}
    chmod 1777 "$ROOTFS_DIR/tmp"

    cat > "$ROOTFS_DIR/etc/os-release" << 'EOF'
NAME="Tinexus OS"
VERSION="1.0"
ID=tinexus
PRETTY_NAME="Tinexus OS 1.0 (Live)"
HOME_URL="https://github.com/itzabhishekgour/TinexusShell"
EOF
    echo "tinexus-live" > "$ROOTFS_DIR/etc/hostname"
    echo "tmpfs   /tmp    tmpfs   defaults,nosuid,nodev   0 0" > "$ROOTFS_DIR/etc/fstab"

    local staged=0
    local tinexus_build="$BUILD_DIR/debug"
    if [ -d "$tinexus_build" ]; then
        while IFS= read -r -d '' candidate; do
            if file "$candidate" | grep -q "ELF.*executable\|ELF.*shared object"; then
                if [[ "$candidate" == *"tinexus-"* ]] || [[ "$candidate" == *"libtinexus"* ]]; then
                    local dest_dir="$ROOTFS_DIR/usr/bin"
                    [[ "$candidate" == *".so"* ]] && dest_dir="$ROOTFS_DIR/usr/lib"
                    mkdir -p "$dest_dir"
                    cp -L "$candidate" "$dest_dir/"
                    staged=$((staged + 1))
                    
                    ldd "$candidate" 2>/dev/null | sed -n 's/.*=> \(.*\) (0x.*/\1/p' | while read -r lib; do
                        [ -f "$lib" ] && { mkdir -p "$ROOTFS_DIR$(dirname "$lib")"; cp -L "$lib" "$ROOTFS_DIR$lib" 2>/dev/null || true; }
                    done
                    ldd "$candidate" 2>/dev/null | sed -n 's/^[[:space:]]*\(\/.*\) (0x.*/\1/p' | while read -r ld_loader; do
                        [ -f "$ld_loader" ] && { mkdir -p "$ROOTFS_DIR$(dirname "$ld_loader")"; cp -L "$ld_loader" "$ROOTFS_DIR$ld_loader" 2>/dev/null || true; }
                    done
                fi
            fi
        done < <(find "$tinexus_build" -maxdepth 4 \( -type f -o -type l \) \( -name 'tinexus-*' -o -name 'libtinexus*.so*' \) -print0 2>/dev/null)
    fi
    success "Staged $staged Tinexus ELF binaries and dependencies."

    if [ -f "$ROOTFS_DIR/usr/bin/tinexus-serviced" ]; then
        ln -sf /usr/bin/tinexus-serviced "$ROOTFS_DIR/sbin/init"
    else
        warn "tinexus-serviced not found in rootfs! System may not boot properly."
    fi

    info "Compressing rootfs with mksquashfs..."
    mkdir -p "$ISO_TREE/live"
    export SQUASHFS_OUT="$ISO_TREE/live/rootfs.squashfs"
    mksquashfs "$ROOTFS_DIR" "$SQUASHFS_OUT" -comp xz -b 1048576 -noappend -quiet
    if ! file -b "$SQUASHFS_OUT" | grep -qi "Squashfs"; then fatal "SquashFS generation failed."; fi
    success "SquashFS rootfs created: $(du -sh "$SQUASHFS_OUT" | cut -f1)"
}

# ── Initramfs Generation ──────────────────────────────────────────────────────
build_initramfs() {
    info "Preparing Initramfs..."
    mkdir -p "$ISO_TREE/boot" "$WORK_DIR"
    local supplied_initrd="$KERNEL_DIR/initramfs.img"
    export INITRAMFS_OUT="$ISO_TREE/boot/initramfs.img"

    if [ -f "$supplied_initrd" ]; then
        if ! file -b "$supplied_initrd" | grep -qiE "cpio|gzip|compress|Zstandard|XZ|LZ4"; then
            fatal "$supplied_initrd is not a valid archive."
        fi
        cp "$supplied_initrd" "$INITRAMFS_OUT"
    else
        info "Building minimal cpio initramfs..."
        local init_staging="$WORK_DIR/initramfs_staging"
        mkdir -p "$init_staging"/{bin,sbin,dev,proc,sys,mnt,newroot,live}

        if command -v busybox &>/dev/null; then
            cp "$(command -v busybox)" "$init_staging/bin/busybox"
            for cmd in sh cat ls mkdir mount umount mdev switch_root sleep; do ln -sf busybox "$init_staging/bin/$cmd" || true; done
        else
            cp "$(command -v sh || echo /bin/sh)" "$init_staging/bin/sh" || true
        fi

        cat > "$init_staging/init" << 'EOINIT'
#!/bin/sh
/bin/mount -t proc proc /proc 2>/dev/null
/bin/mount -t sysfs sysfs /sys 2>/dev/null
/bin/mount -t devtmpfs devtmpfs /dev 2>/dev/null || /bin/mdev -s 2>/dev/null

echo "Tinexus: Searching for live rootfs..."
for attempt in 1 2 3 4 5; do
    for dev in /dev/sr0 /dev/sda /dev/sdb /dev/vda; do
        [ -b "$dev" ] && /bin/mount -o ro "$dev" /mnt 2>/dev/null && break 2
    done
    sleep 1
done

if [ -f /mnt/live/rootfs.squashfs ]; then
    /bin/mount -t squashfs -o ro /mnt/live/rootfs.squashfs /newroot
    exec switch_root /newroot /sbin/init
else
    echo "Tinexus: FATAL — rootfs.squashfs not found. Dropping to emergency shell."
    exec /bin/sh
fi
EOINIT
        chmod 0755 "$init_staging/init"
        (cd "$init_staging" && find . | cpio -o -H newc 2>/dev/null | gzip -9 > "$INITRAMFS_OUT")
    fi
    success "Initramfs ready: $(du -sh "$INITRAMFS_OUT" | cut -f1)"

    cp "$VMLINUZ" "$ISO_TREE/boot/vmlinuz"
    success "Kernel staged: $(du -sh "$ISO_TREE/boot/vmlinuz" | cut -f1)"
}

# ── GRUB Configurations ───────────────────────────────────────────────────────
generate_grub_config() {
    mkdir -p "$ISO_TREE/boot/grub"
    cat > "$ISO_TREE/boot/grub/grub.cfg" << 'EOGRUB'
set default=0
set timeout=5
insmod all_video
insmod gfxterm
insmod iso9660
terminal_output gfxterm

menuentry "Tinexus OS Live (Wayland Desktop)" {
    linux   /boot/vmlinuz root=live:CDLABEL=TINEXUS_LIVE boot=live rd.live.image rd.live.dir=/live rd.live.squashimg=rootfs.squashfs
    initrd  /boot/initramfs.img
}

menuentry "Tinexus OS Live (Safe Graphics / nomodeset)" {
    linux   /boot/vmlinuz root=live:CDLABEL=TINEXUS_LIVE boot=live rd.live.image rd.live.dir=/live rd.live.squashimg=rootfs.squashfs nomodeset
    initrd  /boot/initramfs.img
}

menuentry "Tinexus OS Live (Debug Console)" {
    linux   /boot/vmlinuz root=live:CDLABEL=TINEXUS_LIVE boot=live rd.live.image rd.live.dir=/live rd.live.squashimg=rootfs.squashfs console=ttyS0,115200n8
    initrd  /boot/initramfs.img
}
EOGRUB

    export EMBEDDED_CFG="$WORK_DIR/grub_embedded.cfg"
    cat > "$EMBEDDED_CFG" << 'EOEMBEDDED'
search --no-floppy --set=root --label TINEXUS_LIVE
set prefix=($root)/boot/grub
configfile $prefix/grub.cfg
EOEMBEDDED
}

# ── GRUB EFI Generation ───────────────────────────────────────────────────────
build_grub_efi_x64() {
    info "Building GRUB EFI image (BOOTX64.EFI)..."
    mkdir -p "$ISO_TREE/EFI/BOOT"
    local efi_modules=(
        part_gpt part_msdos fat exfat iso9660
        normal boot linux linux16 configfile
        search search_fs_uuid search_fs_file search_label
        gfxterm gfxterm_background all_video video_fb video efi_gop
        echo test true ls cat reboot halt
    )

    grub-mkimage -O x86_64-efi \
        -d "$GRUB_EFI_MODS" \
        -p /boot/grub \
        -c "$EMBEDDED_CFG" \
        -o "$ISO_TREE/EFI/BOOT/BOOTX64.EFI" \
        "${efi_modules[@]}"

    if ! file -b "$ISO_TREE/EFI/BOOT/BOOTX64.EFI" | grep -qi "PE32\|EFI"; then
        fatal "BOOTX64.EFI is not a valid PE32+ EFI binary."
    fi
    success "BOOTX64.EFI generated."
}

# ── GRUB BIOS Generation ──────────────────────────────────────────────────────
build_grub_bios() {
    info "Building GRUB BIOS image (eltorito.img)..."
    mkdir -p "$ISO_TREE/boot/grub/i386-pc"
    local bios_modules=(
        biosdisk iso9660 normal linux search search_label search_fs_uuid
        configfile echo test reboot halt gfxterm all_video video video_fb
    )

    grub-mkimage -O i386-pc \
        -d "$GRUB_BIOS_MODS" \
        -p /boot/grub \
        -c "$EMBEDDED_CFG" \
        -o "$WORK_DIR/core.img" \
        "${bios_modules[@]}"

    local cdboot="$GRUB_BIOS_MODS/cdboot.img"
    [ -f "$cdboot" ] || fatal "cdboot.img missing from $GRUB_BIOS_MODS"
    cat "$cdboot" "$WORK_DIR/core.img" > "$ISO_TREE/boot/grub/i386-pc/eltorito.img"
    
    export BIOS_BOOT_IMG="$GRUB_BIOS_MODS/boot_hybrid.img"
    [ -f "$BIOS_BOOT_IMG" ] || fatal "boot_hybrid.img missing from $GRUB_BIOS_MODS"
    
    for mod in "${bios_modules[@]}"; do
        [ -f "$GRUB_BIOS_MODS/${mod}.mod" ] && cp "$GRUB_BIOS_MODS/${mod}.mod" "$ISO_TREE/boot/grub/i386-pc/" || true
    done
    success "BIOS eltorito.img and hybrid MBR staged."
}

# ── EFI System Partition ──────────────────────────────────────────────────────
build_esp() {
    info "Building EFI System Partition (esp.img)..."
    export ESP_IMG="$WORK_DIR/esp.img"
    dd if=/dev/zero of="$ESP_IMG" bs=1K count=4096 status=none
    mkfs.vfat "$ESP_IMG" -n "TINEXUS_EFI" >/dev/null
    mmd -i "$ESP_IMG" ::/EFI ::/EFI/BOOT
    mcopy -i "$ESP_IMG" "$ISO_TREE/EFI/BOOT/BOOTX64.EFI" ::/EFI/BOOT/BOOTX64.EFI
    success "EFI System Partition created."
}

# ── ISO Assembly ──────────────────────────────────────────────────────────────
assemble_iso() {
    info "Assembling Hybrid ISO with xorriso..."
    mkdir -p "$BUILD_DIR"
    
    xorriso -as mkisofs \
        -r \
        -V "TINEXUS_LIVE" \
        -J \
        -joliet-long \
        -b boot/grub/i386-pc/eltorito.img \
        -no-emul-boot \
        -boot-load-size 4 \
        -boot-info-table \
        -isohybrid-mbr "$BIOS_BOOT_IMG" \
        -eltorito-alt-boot \
        -append_partition 2 28732ac11ff8d211ba4b00a0c93ec93b "$ESP_IMG" \
        -appended_part_as_gpt \
        -c /boot.catalog \
        -e '--interval:appended_partition_2:all::' \
        -no-emul-boot \
        -o "$OUTPUT_ISO" \
        "$ISO_TREE" >/dev/null 2>&1

    success "ISO Assembled: $OUTPUT_ISO"
}

# ── Validation Framework ──────────────────────────────────────────────────────
validate_iso() {
    info "Running Strict ISO Validation..."

    [ -f "$OUTPUT_ISO" ] || fatal "ISO file missing."
    file -b "$OUTPUT_ISO" | grep -qi "ISO 9660\|CD-ROM" || fatal "Not a valid ISO 9660 file."
    
    local eltorito_report
    eltorito_report=$(xorriso -indev "$OUTPUT_ISO" -report_el_torito plain 2>&1)
    
    echo "$eltorito_report" | grep -q "80x86\|0x00" || fatal "BIOS boot entry missing from El Torito catalog."
    echo "$eltorito_report" | grep -q "UEFI" || fatal "UEFI boot entry missing from El Torito catalog."

    [ -s "$ISO_TREE/boot/grub/i386-pc/eltorito.img" ] || fatal "eltorito.img is empty."
    [ -f "$ISO_TREE/EFI/BOOT/BOOTX64.EFI" ] || fatal "BOOTX64.EFI missing."
    [ -f "$ISO_TREE/live/rootfs.squashfs" ] || fatal "rootfs.squashfs missing."
    [ -f "$ISO_TREE/boot/initramfs.img" ] || fatal "initramfs missing."
    [ -f "$ISO_TREE/boot/vmlinuz" ] || fatal "vmlinuz missing."
    [ -f "$ISO_TREE/boot/grub/grub.cfg" ] || fatal "grub.cfg missing."

    (cd "$BUILD_DIR" && sha256sum "$(basename "$OUTPUT_ISO")" > "$(basename "$OUTPUT_ISO").sha256")
    
    success "All checks passed! The ISO is a true BIOS+UEFI Hybrid."
    info "SHA256: $(cat "$BUILD_DIR/$(basename "$OUTPUT_ISO").sha256")"
}

# ── QEMU Smoke Test ───────────────────────────────────────────────────────────
run_smoke_test() {
    info "Launching QEMU BIOS Smoke Test..."
    qemu-system-x86_64 -m 2G -cdrom "$OUTPUT_ISO" -boot d -vga virtio &
    local qemu_pid=$!
    info "QEMU running with PID $qemu_pid. Close window to exit."
    wait $qemu_pid || true
}

# ── Main ──────────────────────────────────────────────────────────────────────
echo -e "\n\e[1;36m======================================================================\e[0m"
echo -e "\e[1;36m  Tinexus OS — Production Hybrid ISO Builder\e[0m"
echo -e "\e[1;36m======================================================================\e[0m\n"

verify_env
build_rootfs
build_initramfs
generate_grub_config
build_grub_efi_x64
build_grub_bios
build_esp
assemble_iso
validate_iso

if [ $SMOKE_TEST -eq 1 ]; then
    run_smoke_test
fi
