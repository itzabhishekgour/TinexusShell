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
    mkdir -p "$ROOTFS_DIR"/{bin,sbin,lib,lib64,usr/{bin,sbin,lib},etc,var/{log,run},run,dev,proc,sys,tmp,mnt,newroot,live,root,home}
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
    done < <(find "$BUILD_DIR" -maxdepth 6 \( -type f -o -type l \) \( -name 'tinexus-*' -o -name 'libtinexus*.so*' \) -print0 2>/dev/null)
    success "Staged $staged Tinexus ELF binaries and dependencies."

    # Create init symlinks pointing to tinexus-serviced (Supervisor PID 1)
    mkdir -p "$ROOTFS_DIR/sbin" "$ROOTFS_DIR/bin"
    ln -sf /usr/bin/tinexus-serviced "$ROOTFS_DIR/sbin/init"
    ln -sf /usr/bin/tinexus-serviced "$ROOTFS_DIR/init"

    # Ensure busybox/sh is available in the rootfs for standard library system() calls
    local bb_bin="$(command -v busybox || command -v sh || echo /bin/sh)"
    if [ -f "$bb_bin" ]; then
        cp -L "$bb_bin" "$ROOTFS_DIR/bin/busybox"
        for cmd in sh cat ls mkdir mount umount mdev sleep; do 
            ln -sf busybox "$ROOTFS_DIR/bin/$cmd" || true
        done
        (ldd "$bb_bin" 2>/dev/null || true) | sed -n 's/.*=> \(.*\) (0x.*/\1/p' | while read -r lib; do
            [ -f "$lib" ] && { mkdir -p "$ROOTFS_DIR$(dirname "$lib")"; cp -L "$lib" "$ROOTFS_DIR$lib" 2>/dev/null || true; }
        done
        (ldd "$bb_bin" 2>/dev/null || true) | sed -n 's/^[[:space:]]*\(\/.*\) (0x.*/\1/p' | while read -r ld_loader; do
            [ -f "$ld_loader" ] && { mkdir -p "$ROOTFS_DIR$(dirname "$ld_loader")"; cp -L "$ld_loader" "$ROOTFS_DIR$ld_loader" 2>/dev/null || true; }
        done
    fi

    # Stage kmod and its dependencies so we can load kernel modules manually
    info "Staging kmod for kernel module loading..."
    if [ -f "/usr/bin/kmod" ]; then
        cp -L "/usr/bin/kmod" "$ROOTFS_DIR/usr/bin/"
        ln -sf kmod "$ROOTFS_DIR/usr/bin/modprobe"
        ldd "/usr/bin/kmod" 2>/dev/null | sed -n 's/.*=> \(.*\) (0x.*/\1/p' | while read -r lib; do
            [ -f "$lib" ] && { mkdir -p "$ROOTFS_DIR$(dirname "$lib")"; cp -L "$lib" "$ROOTFS_DIR$lib" 2>/dev/null || true; }
        done
        ldd "/usr/bin/kmod" 2>/dev/null | sed -n 's/^[[:space:]]*\(\/.*\) (0x.*/\1/p' | while read -r ld_loader; do
            [ -f "$ld_loader" ] && { mkdir -p "$ROOTFS_DIR$(dirname "$ld_loader")"; cp -L "$ld_loader" "$ROOTFS_DIR$ld_loader" 2>/dev/null || true; }
        done
    fi

    info "Staging udevd and udevadm for input device detection (libinput requirement)..."
    if [ -f "/usr/bin/udevadm" ]; then
        cp -L "/usr/bin/udevadm" "$ROOTFS_DIR/usr/bin/"
        mkdir -p "$ROOTFS_DIR/lib/systemd" "$ROOTFS_DIR/lib/udev" "$ROOTFS_DIR/usr/lib/udev" "$ROOTFS_DIR/usr/lib/systemd" "$ROOTFS_DIR/etc/udev"
        cp -L "/lib/systemd/systemd-udevd" "$ROOTFS_DIR/lib/systemd/" 2>/dev/null || true
        cp -L "/lib/systemd/systemd-udevd" "$ROOTFS_DIR/usr/lib/systemd/" 2>/dev/null || true
        cp -r /lib/udev/* "$ROOTFS_DIR/lib/udev/" 2>/dev/null || true
        cp -r /lib/udev/* "$ROOTFS_DIR/usr/lib/udev/" 2>/dev/null || true
        cp -r /etc/udev/* "$ROOTFS_DIR/etc/udev/" 2>/dev/null || true
        mkdir -p "$ROOTFS_DIR/usr/share/libinput" "$ROOTFS_DIR/usr/share/X11/xkb" "$ROOTFS_DIR/etc/libinput"
        cp -r /usr/share/libinput/* "$ROOTFS_DIR/usr/share/libinput/" 2>/dev/null || true
        cp -r /usr/share/X11/xkb/* "$ROOTFS_DIR/usr/share/X11/xkb/" 2>/dev/null || true
        cp -r /etc/libinput/* "$ROOTFS_DIR/etc/libinput/" 2>/dev/null || true
        mkdir -p "$ROOTFS_DIR/etc/udev/rules.d" "$ROOTFS_DIR/lib/udev/rules.d" "$ROOTFS_DIR/usr/lib/udev/rules.d"
        cat << 'EOF_UDEV_SEAT' > "$ROOTFS_DIR/etc/udev/rules.d/99-tinexus-seat.rules"
SUBSYSTEM=="input", ENV{ID_INPUT}=="1", ENV{ID_SEAT}="seat0", TAG+="seat", TAG+="seat0", TAG+="uaccess"
SUBSYSTEM=="drm", KERNEL=="card[0-9]*", ENV{ID_SEAT}="seat0", TAG+="seat", TAG+="seat0", TAG+="master-of-seat", TAG+="uaccess"
EOF_UDEV_SEAT
        cp -L "$ROOTFS_DIR/etc/udev/rules.d/99-tinexus-seat.rules" "$ROOTFS_DIR/lib/udev/rules.d/"
        cp -L "$ROOTFS_DIR/etc/udev/rules.d/99-tinexus-seat.rules" "$ROOTFS_DIR/usr/lib/udev/rules.d/"
        for bin in "/usr/bin/udevadm" "/lib/systemd/systemd-udevd"; do
            [ -f "$bin" ] && ldd "$bin" 2>/dev/null | sed -n 's/.*=> \(.*\) (0x.*/\1/p' | while read -r lib; do
                [ -f "$lib" ] && { mkdir -p "$ROOTFS_DIR$(dirname "$lib")"; cp -L "$lib" "$ROOTFS_DIR$lib" 2>/dev/null || true; }
            done
            [ -f "$bin" ] && ldd "$bin" 2>/dev/null | sed -n 's/^[[:space:]]*\(\/.*\) (0x.*/\1/p' | while read -r ld_loader; do
                [ -f "$ld_loader" ] && { mkdir -p "$ROOTFS_DIR$(dirname "$ld_loader")"; cp -L "$ld_loader" "$ROOTFS_DIR$ld_loader" 2>/dev/null || true; }
            done
        done
    fi

    # ── Stage foot terminal + fonts + fontconfig ──────────────────────────────
    info "Staging foot terminal and font stack..."
    local foot_bin
    foot_bin="$(command -v foot 2>/dev/null || true)"
    if [ -n "$foot_bin" ] && [ -f "$foot_bin" ]; then
        cp -L "$foot_bin" "$ROOTFS_DIR/usr/bin/foot"
        ldd "$foot_bin" 2>/dev/null | sed -n 's/.*=> \(.*\) (0x.*/\1/p' | while read -r lib; do
            [ -f "$lib" ] && { mkdir -p "$ROOTFS_DIR$(dirname "$lib")"; cp -L "$lib" "$ROOTFS_DIR$lib" 2>/dev/null || true; }
        done
        ldd "$foot_bin" 2>/dev/null | sed -n 's/^[[:space:]]*\(\/.*\) (0x.*/\1/p' | while read -r ld; do
            [ -f "$ld" ] && { mkdir -p "$ROOTFS_DIR$(dirname "$ld")"; cp -L "$ld" "$ROOTFS_DIR$ld" 2>/dev/null || true; }
        done
        # terminfo entry (foot needs its own)
        if [ -d "/usr/share/terminfo/f" ]; then
            mkdir -p "$ROOTFS_DIR/usr/share/terminfo/f"
            cp -r /usr/share/terminfo/f/. "$ROOTFS_DIR/usr/share/terminfo/f/" 2>/dev/null || true
        fi
        success "foot binary staged."
    else
        warn "foot not found on host — terminal launch will fall back to weston-terminal/alacritty."
    fi

    # Fonts — without these foot renders blank text or crashes
    info "Staging fonts for foot terminal..."
    mkdir -p "$ROOTFS_DIR/usr/share/fonts"
    for font_dir in \
        "/usr/share/fonts/truetype/dejavu" \
        "/usr/share/fonts/truetype/liberation" \
        "/usr/share/fonts/truetype/noto" \
        "/usr/share/fonts/opentype/noto" \
        "/usr/share/fonts/X11/misc"; do
        if [ -d "$font_dir" ]; then
            local dest="$ROOTFS_DIR${font_dir}"
            mkdir -p "$dest"
            cp -r "${font_dir}/." "$dest/" 2>/dev/null || true
        fi
    done

    # fontconfig — so foot can discover fonts at runtime
    info "Staging fontconfig..."
    if [ -d "/etc/fonts" ]; then
        cp -r /etc/fonts "$ROOTFS_DIR/etc/" 2>/dev/null || true
    fi
    if [ -d "/usr/share/fontconfig" ]; then
        mkdir -p "$ROOTFS_DIR/usr/share/fontconfig"
        cp -r /usr/share/fontconfig/. "$ROOTFS_DIR/usr/share/fontconfig/" 2>/dev/null || true
    fi
    if [ -d "/var/cache/fontconfig" ]; then
        mkdir -p "$ROOTFS_DIR/var/cache/fontconfig"
        cp -r /var/cache/fontconfig/. "$ROOTFS_DIR/var/cache/fontconfig/" 2>/dev/null || true
    fi

    # Stage custom wallpaper image from Temp directory
    info "Staging custom wallpaper image into rootfs..."
    mkdir -p "$ROOTFS_DIR/usr/share/backgrounds"
    if [ -f "$PROJECT_DIR/Temp/tinexus-default.jpg" ]; then
        cp -L "$PROJECT_DIR/Temp/tinexus-default.jpg" "$ROOTFS_DIR/usr/share/backgrounds/tinexus-default.jpg"
        success "Staged 1080p mountain wallpaper (tinexus-default.jpg)."
    elif [ -f "$PROJECT_DIR/Temp/daniel-leone-v7daTKlZzaw-unsplash.jpg" ]; then
        cp -L "$PROJECT_DIR/Temp/daniel-leone-v7daTKlZzaw-unsplash.jpg" "$ROOTFS_DIR/usr/share/backgrounds/tinexus-default.jpg"
        success "Staged custom mountain wallpaper."
    fi

    # Locale — foot uses LC_ALL/LANG; stage minimal en_US.UTF-8
    info "Staging locale data (en_US.UTF-8)..."
    if [ -d "/usr/lib/locale" ]; then
        mkdir -p "$ROOTFS_DIR/usr/lib/locale"
        cp -r /usr/lib/locale/en_US.utf8 "$ROOTFS_DIR/usr/lib/locale/" 2>/dev/null || true
        [ -f "/usr/lib/locale/locale-archive" ] && cp /usr/lib/locale/locale-archive "$ROOTFS_DIR/usr/lib/locale/" 2>/dev/null || true
    fi
    mkdir -p "$ROOTFS_DIR/etc"
    echo "LANG=en_US.UTF-8" > "$ROOTFS_DIR/etc/locale.conf"

    # PAM — required by tinexus-lock for authentication
    info "Staging PAM libraries and configuration for tinexus-lock..."
    mkdir -p "$ROOTFS_DIR/lib/security" "$ROOTFS_DIR/usr/lib/security" \
             "$ROOTFS_DIR/etc/pam.d" "$ROOTFS_DIR/etc/security"
    # Copy PAM modules (pam_unix.so, pam_permit.so, etc.)
    for pam_dir in "/lib/security" "/lib/x86_64-linux-gnu/security" \
                   "/usr/lib/security" "/usr/lib/x86_64-linux-gnu/security"; do
        if [ -d "$pam_dir" ]; then
            cp -r "${pam_dir}/." "$ROOTFS_DIR/lib/security/" 2>/dev/null || true
            cp -r "${pam_dir}/." "$ROOTFS_DIR/usr/lib/security/" 2>/dev/null || true
        fi
    done
    # Copy PAM config for 'login' (tinexus-lock calls pam_start("login", ...))
    if [ -f "/etc/pam.d/login" ]; then
        cp /etc/pam.d/login "$ROOTFS_DIR/etc/pam.d/login"
    else
        # Minimal fallback PAM config
        cat > "$ROOTFS_DIR/etc/pam.d/login" << 'EOF_PAM'
auth       required   pam_unix.so
account    required   pam_unix.so
session    required   pam_unix.so
EOF_PAM
    fi
    # Common PAM includes
    for f in common-auth common-account common-session; do
        [ -f "/etc/pam.d/$f" ] && cp "/etc/pam.d/$f" "$ROOTFS_DIR/etc/pam.d/$f" 2>/dev/null || true
    done
    # /etc/passwd and /etc/shadow needed for pam_unix
    [ -f /etc/passwd ] && cp /etc/passwd "$ROOTFS_DIR/etc/passwd" 2>/dev/null || true
    [ -f /etc/group ]  && cp /etc/group  "$ROOTFS_DIR/etc/group"  2>/dev/null || true
    [ -f /etc/shadow ] && cp /etc/shadow "$ROOTFS_DIR/etc/shadow" 2>/dev/null || true

    info "Staging minimal kernel modules into rootfs for runtime hardware support..."
    mkdir -p "$ROOTFS_DIR/lib/modules"
    if [ -d "/lib/modules/7.0.0-28-generic" ]; then
        find "/lib/modules/7.0.0-28-generic" -type f \( \
            -name "isofs.ko*" -o -name "ahci.ko*" -o -name "libahci.ko*" \
            -o -name "virtio-gpu.ko*" -o -name "virtio_dma_buf.ko*" -o -name "bochs.ko*" \
            -o -name "virtio_input.ko*" \
            -o -name "evdev.ko*" \
            -o -name "hid.ko*" -o -name "hid-generic.ko*" -o -name "usbhid.ko*" \
        \) | while read -r mod; do
            cp -L "$mod" "$ROOTFS_DIR/lib/modules/"
        done
        for compressed in "$ROOTFS_DIR/lib/modules"/*.zst; do
            [ -f "$compressed" ] && zstd -d --rm "$compressed" 2>/dev/null || true
        done
        if [ -f "/usr/sbin/depmod" ]; then
            /usr/sbin/depmod -b "$ROOTFS_DIR" 7.0.0-28-generic 2>/dev/null || true
        fi
    fi

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

    info "Building minimal Tinexus cpio initramfs..."
    local init_staging="$WORK_DIR/initramfs_staging"
    mkdir -p "$init_staging"/{bin,sbin,dev,proc,sys,mnt,newroot,live,tmp}

    # Stage tinexus-splash and logo
    if [ -f "$BUILD_DIR/tinexus-splash" ]; then
        cp -L "$BUILD_DIR/tinexus-splash" "$init_staging/bin/"
        (ldd "$BUILD_DIR/tinexus-splash" 2>/dev/null || true) | sed -n 's/.*=> \(.*\) (0x.*/\1/p' | while read -r lib; do
            [ -f "$lib" ] && { mkdir -p "$init_staging$(dirname "$lib")"; cp -L "$lib" "$init_staging$lib" 2>/dev/null || true; }
        done
        (ldd "$BUILD_DIR/tinexus-splash" 2>/dev/null || true) | sed -n 's/^[[:space:]]*\(\/.*\) (0x.*/\1/p' | while read -r ld_loader; do
            [ -f "$ld_loader" ] && { mkdir -p "$init_staging$(dirname "$ld_loader")"; cp -L "$ld_loader" "$init_staging$ld_loader" 2>/dev/null || true; }
        done
    else
        warn "tinexus-splash binary not found!"
    fi
    if [ -f "$PROJECT_DIR/Temp/tinexus-logo.png" ]; then
        cp -L "$PROJECT_DIR/Temp/tinexus-logo.png" "$init_staging/"
    fi

    local bb_bin="$(command -v busybox || command -v sh || echo /bin/sh)"
    cp -L "$bb_bin" "$init_staging/bin/busybox"
    for cmd in sh cat ls mkdir mount umount mdev switch_root sleep; do ln -sf busybox "$init_staging/bin/$cmd" || true; done

    (ldd "$bb_bin" 2>/dev/null || true) | sed -n 's/.*=> \(.*\) (0x.*/\1/p' | while read -r lib; do
        [ -f "$lib" ] && { mkdir -p "$init_staging$(dirname "$lib")"; cp -L "$lib" "$init_staging$lib" 2>/dev/null || true; }
    done
    (ldd "$bb_bin" 2>/dev/null || true) | sed -n 's/^[[:space:]]*\(\/.*\) (0x.*/\1/p' | while read -r ld_loader; do
        [ -f "$ld_loader" ] && { mkdir -p "$init_staging$(dirname "$ld_loader")"; cp -L "$ld_loader" "$init_staging$ld_loader" 2>/dev/null || true; }
    done

    mkdir -p "$init_staging/lib/modules"
    local kver="7.0.0-28-generic"
    if [ -d "/lib/modules/$kver" ]; then
        find "/lib/modules/$kver" -type f \( \
            -name "isofs.ko*" -o -name "ahci.ko*" -o -name "libahci.ko*" \
            -o -name "virtio-gpu.ko*" -o -name "virtio_dma_buf.ko*" -o -name "bochs.ko*" \
            -o -name "virtio_input.ko*" \
            -o -name "evdev.ko*" \
            -o -name "hid.ko*" -o -name "hid-generic.ko*" -o -name "usbhid.ko*" \
        \) | while read -r mod; do
            cp -L "$mod" "$init_staging/lib/modules/"
        done
        for compressed in "$init_staging/lib/modules"/*.zst; do
            [ -f "$compressed" ] && zstd -d --rm "$compressed" 2>/dev/null || true
        done
    fi

    cat > "$init_staging/init" << 'EOINIT'
#!/bin/sh
/bin/mount -t proc proc /proc 2>/dev/null
/bin/mount -t sysfs sysfs /sys 2>/dev/null
/bin/mount -t devtmpfs devtmpfs /dev 2>/dev/null || /bin/mdev -s 2>/dev/null

[ -c /dev/ttyS0 ] && exec >/dev/ttyS0 2>&1

echo "Tinexus OS: Loading storage and graphics kernel modules..."
[ -f /lib/modules/libahci.ko ] && insmod /lib/modules/libahci.ko || true
[ -f /lib/modules/ahci.ko ] && insmod /lib/modules/ahci.ko || true
[ -f /lib/modules/isofs.ko ] && insmod /lib/modules/isofs.ko || true
[ -f /lib/modules/virtio_dma_buf.ko ] && insmod /lib/modules/virtio_dma_buf.ko || true
[ -f /lib/modules/virtio-gpu.ko ] && insmod /lib/modules/virtio-gpu.ko || true
[ -f /lib/modules/virtio_input.ko ] && insmod /lib/modules/virtio_input.ko || true
[ -f /lib/modules/evdev.ko ] && insmod /lib/modules/evdev.ko || true
[ -f /lib/modules/hid.ko ] && insmod /lib/modules/hid.ko || true
[ -f /lib/modules/hid-generic.ko ] && insmod /lib/modules/hid-generic.ko || true
[ -f /lib/modules/usbhid.ko ] && insmod /lib/modules/usbhid.ko || true
[ -f /lib/modules/bochs.ko ] && insmod /lib/modules/bochs.ko || true
for mod in /lib/modules/*.ko; do
    [ -f "$mod" ] && insmod "$mod" 2>/dev/null
done
/bin/mdev -s 2>/dev/null

# Graphics modules loaded, start splash screen
if [ -x /bin/tinexus-splash ]; then
    /bin/tinexus-splash &
    # Give it a moment to create the fifo
    sleep 0.1
    echo 20 > /tmp/splash_progress 2>/dev/null
fi

echo "Tinexus OS: Searching for Live CD rootfs..."
for attempt in 1 2 3 4 5 6 7 8 9 10; do
    echo 30 > /tmp/splash_progress 2>/dev/null
    /bin/mdev -s 2>/dev/null
    for dev in /dev/sr0 /dev/sr1 /dev/sda /dev/sdb /dev/vda /dev/vdb /dev/sg0; do
        if [ -b "$dev" ]; then
            /bin/mount -o ro "$dev" /mnt 2>/dev/null
            if [ -f /mnt/live/rootfs.squashfs ]; then
                echo "Tinexus OS: Found rootfs on $dev!"
                echo 50 > /tmp/splash_progress 2>/dev/null
                break 2
            else
                /bin/umount /mnt 2>/dev/null
            fi
        fi
    done
    sleep 1
done

if [ -f /mnt/live/rootfs.squashfs ]; then
    echo "Tinexus OS: Mounting SquashFS rootfs..."
    echo 70 > /tmp/splash_progress 2>/dev/null
    /bin/mount -t squashfs -o ro /mnt/live/rootfs.squashfs /newroot
    /bin/mount -t devtmpfs devtmpfs /newroot/dev 2>/dev/null || true
    mkdir -p /newroot/dev/shm
    /bin/mount -t tmpfs tmpfs /newroot/dev/shm 2>/dev/null || true
    /bin/mount -t proc proc /newroot/proc 2>/dev/null || true
    /bin/mount -t sysfs sysfs /newroot/sys 2>/dev/null || true
    /bin/mount -t tmpfs tmpfs /newroot/run 2>/dev/null || true
    /bin/mount -t tmpfs tmpfs /newroot/tmp 2>/dev/null || true
    /bin/mount -t tmpfs tmpfs /newroot/var 2>/dev/null || true
    /bin/mount -t tmpfs tmpfs /newroot/root 2>/dev/null || true
    /bin/mount -t tmpfs tmpfs /newroot/home 2>/dev/null || true
    
    echo 90 > /tmp/splash_progress 2>/dev/null
    echo "Tinexus OS: Switching to Tinexus Serviced Init..."
    
    # We must keep the splash daemon running until wayland starts,
    # but switch_root will kill processes holding handles to old root.
    # To fix this, we signal 100% so it can exit on its own.
    echo 100 > /tmp/splash_progress 2>/dev/null
    sleep 0.2
    
    exec switch_root /newroot /usr/bin/tinexus-serviced
else
    echo "Tinexus OS: FATAL — rootfs.squashfs not found. Dropping to emergency shell."
    exec /bin/sh
fi
EOINIT
    chmod 0755 "$init_staging/init"
    (cd "$init_staging" && find . | cpio -o -H newc 2>/dev/null | gzip -9 > "$INITRAMFS_OUT")
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
    linux   /boot/vmlinuz root=live:CDLABEL=TINEXUS_LIVE boot=live rd.live.image rd.live.dir=/live rd.live.squashimg=rootfs.squashfs quiet loglevel=3 vt.global_cursor_default=0 logo.nologo fbcon=nodefer console=ttyS0,115200n8
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
