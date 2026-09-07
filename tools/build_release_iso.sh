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
    echo "Tinexus-Desktop" > "$ROOTFS_DIR/etc/hostname"
    echo "tmpfs   /tmp    tmpfs   defaults,nosuid,nodev   0 0" > "$ROOTFS_DIR/etc/fstab"

    # Create minimal users and groups for Tinexus
    cat > "$ROOTFS_DIR/etc/passwd" << 'EOF'
root:x:0:0:root:/root:/bin/sh
messagebus:x:104:104::/var/run/dbus:/bin/false
tinexus:x:1000:1000:Tinexus User:/home/tinexus:/bin/sh
EOF
    cat > "$ROOTFS_DIR/etc/group" << 'EOF'
root:x:0:
messagebus:x:104:
tinexus:x:1000:
tty:x:5:
audio:x:29:tinexus
video:x:44:tinexus
input:x:104:tinexus
render:x:110:tinexus
EOF
    cat > "$ROOTFS_DIR/etc/shadow" << 'EOF'
root::10933:0:99999:7:::
tinexus::10933:0:99999:7:::
EOF
    mkdir -p "$ROOTFS_DIR/home/tinexus"
    # chown won't work correctly without fakeroot/sudo unless we're root, but we can try or let init do it.
    # We will let init handle the runtime ownership if needed, or just set it to 1000:1000
    chown 1000:1000 "$ROOTFS_DIR/home/tinexus" || true

    # Create nsswitch.conf and pull in glibc NSS libraries so getpwuid() can actually read /etc/passwd
    cat > "$ROOTFS_DIR/etc/nsswitch.conf" << 'EOF'
passwd:         files
group:          files
shadow:         files
hosts:          files dns
networks:       files
protocols:      files
services:       files
ethers:         files
rpc:            files
EOF
    mkdir -p "$ROOTFS_DIR/lib/x86_64-linux-gnu/"
    cp -P /lib/x86_64-linux-gnu/libnss_files.so* "$ROOTFS_DIR/lib/x86_64-linux-gnu/" 2>/dev/null || true
    cp -P /lib/x86_64-linux-gnu/libnss_compat.so* "$ROOTFS_DIR/lib/x86_64-linux-gnu/" 2>/dev/null || true

    local staged=0
    while IFS= read -r -d '' candidate; do
        if [[ "$candidate" == *"/debug/"* ]] || [[ "$candidate" == *"/txui_verify/"* ]]; then continue; fi
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
    done < <(find "$BUILD_DIR/bin" -maxdepth 1 \( -type f -o -type l \) \( -name 'tinexus-*' -o -name 'libtinexus*.so*' \) -print0 2>/dev/null)
    
    # Force copy all shared libraries and their version symlinks to /usr/lib to fix broken RUNPATHs
    find "$BUILD_DIR/lib" -maxdepth 1 \( -type f -o -type l \) -name "libtinexus*.so*" -exec cp -a {} "$ROOTFS_DIR/usr/lib/" \; 2>/dev/null || true
    
    success "Staged $staged Tinexus ELF binaries and dependencies."

    # Generate .desktop file for Tinexus App Installer so it appears in the Launcher
    mkdir -p "$ROOTFS_DIR/usr/share/applications"
    cat > "$ROOTFS_DIR/usr/share/applications/tinexus-app-installer.desktop" << 'EOF'
[Desktop Entry]
Name=Tinexus App Installer
Comment=Install .txapp packages
Exec=/usr/bin/tinexus-app-installer
Icon=system-software-install
Terminal=false
Type=Application
Categories=System;
EOF

    # Generate .desktop file for Tinexus Files (Miller Column Browser)
    cat > "$ROOTFS_DIR/usr/share/applications/tinexus-files.desktop" << 'EOF'
[Desktop Entry]
Name=Files
Comment=Tinexus File Manager (Phase 2)
Exec=/usr/bin/tinexus-files
Icon=system-file-manager
Terminal=false
Type=Application
Categories=System;Utility;Core;
EOF

    # Generate .desktop file for About Tinexus
    cat > "$ROOTFS_DIR/usr/share/applications/tinexus-about.desktop" << 'EOF'
[Desktop Entry]
Name=About Tinexus
Comment=System Profiler and Hardware Specifications
Exec=/usr/bin/tinexus-about
Icon=tinexus-logo
Terminal=false
Type=Application
Categories=System;Core;
EOF

    # Generate .desktop file for Settings
    cat > "$ROOTFS_DIR/usr/share/applications/tinexus-settings.desktop" << 'EOF'
[Desktop Entry]
Name=Settings
Comment=Tinexus System Settings
Exec=/usr/bin/tinexus-settings-ui
Icon=preferences-system
Terminal=false
Type=Application
Categories=System;Settings;
EOF

    # Create init symlinks pointing to tinexus-serviced (Supervisor PID 1)
    mkdir -p "$ROOTFS_DIR/sbin" "$ROOTFS_DIR/bin" "$ROOTFS_DIR/etc"
    ln -sf /usr/bin/tinexus-serviced "$ROOTFS_DIR/sbin/init"
    ln -sf /usr/bin/tinexus-serviced "$ROOTFS_DIR/init"

    # Set up /etc/profile for a nice shell prompt
    cat > "$ROOTFS_DIR/etc/profile" << 'EOF'
export PS1='\e[01;32m\u@\h\e[00m:\e[01;34m\w\e[00m\$ '
export PATH=/usr/bin:/bin:/usr/sbin:/sbin
EOF

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

    info "Staging AppImage support (FUSE3 and tx-appimage)..."
    if [ -f "/usr/bin/fusermount3" ]; then
        mkdir -p "$ROOTFS_DIR/usr/bin"
        cp -L "/usr/bin/fusermount3" "$ROOTFS_DIR/usr/bin/"
        # FUSE relies on setuid or proper permissions, but inside our session it's often user-mounted.
        chmod +s "$ROOTFS_DIR/usr/bin/fusermount3" 2>/dev/null || true
        
        # Pull in libfuse3.so.3
        ldd "/usr/bin/fusermount3" 2>/dev/null | sed -n 's/.*=> \(.*\) (0x.*/\1/p' | while read -r lib; do
            [ -f "$lib" ] && { mkdir -p "$ROOTFS_DIR$(dirname "$lib")"; cp -L "$lib" "$ROOTFS_DIR$lib" 2>/dev/null || true; }
        done
        ldd "/usr/bin/fusermount3" 2>/dev/null | sed -n 's/^[[:space:]]*\(\/.*\) (0x.*/\1/p' | while read -r ld_loader; do
            [ -f "$ld_loader" ] && { mkdir -p "$ROOTFS_DIR$(dirname "$ld_loader")"; cp -L "$ld_loader" "$ROOTFS_DIR$ld_loader" 2>/dev/null || true; }
        done
    else
        warn "fusermount3 not found on host. AppImages will rely solely on extraction fallback."
    fi

    # Stage the tx-appimage wrapper script
    if [ -f "$PROJECT_DIR/tools/tinexus-appimage-runner.sh" ]; then
        mkdir -p "$ROOTFS_DIR/usr/bin"
        cp -L "$PROJECT_DIR/tools/tinexus-appimage-runner.sh" "$ROOTFS_DIR/usr/bin/tx-appimage"
        chmod +x "$ROOTFS_DIR/usr/bin/tx-appimage"
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

    # ── Stage D-Bus Daemon and Policies ──────────────────────────────
    info "Staging D-Bus Daemon and Policies..."
    if [ -f "/usr/bin/dbus-daemon" ]; then
        cp -L "/usr/bin/dbus-daemon" "$ROOTFS_DIR/usr/bin/"
        [ -f "/usr/bin/dbus-uuidgen" ] && cp -L "/usr/bin/dbus-uuidgen" "$ROOTFS_DIR/usr/bin/"
        
        # Directories
        mkdir -p "$ROOTFS_DIR/var/run/dbus" "$ROOTFS_DIR/var/lib/dbus" "$ROOTFS_DIR/etc/dbus-1/system.d" "$ROOTFS_DIR/usr/share/dbus-1"
        chown 104:104 "$ROOTFS_DIR/var/run/dbus" 2>/dev/null || true
        chown 104:104 "$ROOTFS_DIR/var/lib/dbus" 2>/dev/null || true

        # Conf files
        cp -r /usr/share/dbus-1/* "$ROOTFS_DIR/usr/share/dbus-1/" 2>/dev/null || true
        cp -r /etc/dbus-1/* "$ROOTFS_DIR/etc/dbus-1/" 2>/dev/null || true
        
        # Clean out host-specific policies that reference non-existent users (like polkitd, systemd-network)
        rm -f "$ROOTFS_DIR/etc/dbus-1/system.d/"*.conf 2>/dev/null || true
        rm -f "$ROOTFS_DIR/usr/share/dbus-1/system.d/"*.conf 2>/dev/null || true

        # Add Tinexus Custom Policy for logind mimic
        cat << 'EOF_DBUS_POL' > "$ROOTFS_DIR/etc/dbus-1/system.d/tinexus-logind.conf"
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-BUS Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <policy user="root">
    <allow own="org.freedesktop.login1"/>
    <allow send_destination="org.freedesktop.login1"/>
    <allow receive_sender="org.freedesktop.login1"/>
  </policy>
  <policy context="default">
    <allow send_destination="org.freedesktop.login1"/>
    <allow receive_sender="org.freedesktop.login1"/>
  </policy>
</busconfig>
EOF_DBUS_POL

        # Dependencies
        for bin in "/usr/bin/dbus-daemon" "/usr/bin/dbus-uuidgen"; do
            [ -f "$bin" ] && ldd "$bin" 2>/dev/null | sed -n 's/.*=> \(.*\) (0x.*/\1/p' | while read -r lib; do
                [ -f "$lib" ] && { mkdir -p "$ROOTFS_DIR$(dirname "$lib")"; cp -L "$lib" "$ROOTFS_DIR$lib" 2>/dev/null || true; }
            done
            [ -f "$bin" ] && ldd "$bin" 2>/dev/null | sed -n 's/^[[:space:]]*\(\/.*\) (0x.*/\1/p' | while read -r ld_loader; do
                [ -f "$ld_loader" ] && { mkdir -p "$ROOTFS_DIR$(dirname "$ld_loader")"; cp -L "$ld_loader" "$ROOTFS_DIR$ld_loader" 2>/dev/null || true; }
            done
        done
    else
        warn "dbus-daemon not found on host. System bus will be unavailable."
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

    # ── Tinexus UI Fonts (Inter) — hardcoded path in PixmanBackend.cpp ────────
    # CRITICAL: PixmanBackend::init_freetype() expects Inter-Regular.ttf at
    # /usr/share/tinexus/fonts/Inter-Regular.ttf — no fallback, explicit failure.
    info "Staging Tinexus UI fonts (Inter) into rootfs..."
    mkdir -p "$ROOTFS_DIR/usr/share/tinexus/fonts"
    if [ ! -f "$PROJECT_DIR/assets/fonts/Inter-Regular.ttf" ]; then
        fatal "MISSING: assets/fonts/Inter-Regular.ttf — run: cp Inter-Regular.ttf assets/fonts/"
    fi
    if [ ! -f "$PROJECT_DIR/assets/fonts/Inter-Bold.ttf" ]; then
        fatal "MISSING: assets/fonts/Inter-Bold.ttf — run: cp Inter-Bold.ttf assets/fonts/"
    fi
    cp -v "$PROJECT_DIR/assets/fonts/Inter-Regular.ttf" "$ROOTFS_DIR/usr/share/tinexus/fonts/"
    cp -v "$PROJECT_DIR/assets/fonts/Inter-Bold.ttf"    "$ROOTFS_DIR/usr/share/tinexus/fonts/"
    success "Staged Inter-Regular.ttf + Inter-Bold.ttf → /usr/share/tinexus/fonts/"

    # ── Stage Tinexus Brand Logo & Icons into rootfs ──────────────────────────
    info "Staging Tinexus brand logo and hicolor icons into rootfs..."
    mkdir -p "$ROOTFS_DIR/usr/share/tinexus"
    mkdir -p "$ROOTFS_DIR/usr/share/pixmaps"
    mkdir -p "$ROOTFS_DIR/usr/share/icons/hicolor/scalable/apps"
    mkdir -p "$ROOTFS_DIR/usr/share/icons/hicolor/512x512/apps"
    mkdir -p "$ROOTFS_DIR/usr/share/icons/hicolor/256x256/apps"
    mkdir -p "$ROOTFS_DIR/usr/share/icons/hicolor/128x128/apps"
    mkdir -p "$ROOTFS_DIR/usr/share/icons/hicolor/64x64/apps"
    mkdir -p "$ROOTFS_DIR/usr/share/icons/hicolor/48x48/apps"
    mkdir -p "$ROOTFS_DIR/usr/share/icons/hicolor/32x32/apps"
    mkdir -p "$ROOTFS_DIR/usr/share/icons/hicolor/24x24/apps"
    mkdir -p "$ROOTFS_DIR/usr/share/icons/hicolor/16x16/apps"

    if [ -f "$PROJECT_DIR/assets/logo/tinexus-logo.svg" ]; then
        cp "$PROJECT_DIR/assets/logo/tinexus-logo.svg" "$ROOTFS_DIR/usr/share/tinexus/tinexus-logo.svg"
        cp "$PROJECT_DIR/assets/logo/tinexus-logo.svg" "$ROOTFS_DIR/usr/share/icons/hicolor/scalable/apps/tinexus-logo.svg"
    fi
    if [ -f "$PROJECT_DIR/assets/logo/tinexus-logo.png" ]; then
        cp "$PROJECT_DIR/assets/logo/tinexus-logo.png" "$ROOTFS_DIR/tinexus-logo.png"
        cp "$PROJECT_DIR/assets/logo/tinexus-logo.png" "$ROOTFS_DIR/usr/share/tinexus/tinexus-logo.png"
        cp "$PROJECT_DIR/assets/logo/tinexus-logo.png" "$ROOTFS_DIR/usr/share/pixmaps/tinexus.png"
        cp "$PROJECT_DIR/assets/logo/tinexus-logo.png" "$ROOTFS_DIR/usr/share/pixmaps/tinexus-logo.png"
    fi
    [ -f "$PROJECT_DIR/assets/logo/tinexus-logo-512.png" ] && cp "$PROJECT_DIR/assets/logo/tinexus-logo-512.png" "$ROOTFS_DIR/usr/share/icons/hicolor/512x512/apps/tinexus-logo.png"
    [ -f "$PROJECT_DIR/assets/logo/tinexus-logo-256.png" ] && cp "$PROJECT_DIR/assets/logo/tinexus-logo-256.png" "$ROOTFS_DIR/usr/share/icons/hicolor/256x256/apps/tinexus-logo.png"
    [ -f "$PROJECT_DIR/assets/logo/tinexus-logo-128.png" ] && cp "$PROJECT_DIR/assets/logo/tinexus-logo-128.png" "$ROOTFS_DIR/usr/share/icons/hicolor/128x128/apps/tinexus-logo.png"
    [ -f "$PROJECT_DIR/assets/logo/tinexus-logo-64.png" ]  && cp "$PROJECT_DIR/assets/logo/tinexus-logo-64.png"  "$ROOTFS_DIR/usr/share/icons/hicolor/64x64/apps/tinexus-logo.png"
    [ -f "$PROJECT_DIR/assets/logo/tinexus-logo-48.png" ]  && cp "$PROJECT_DIR/assets/logo/tinexus-logo-48.png"  "$ROOTFS_DIR/usr/share/icons/hicolor/48x48/apps/tinexus-logo.png"
    [ -f "$PROJECT_DIR/assets/logo/tinexus-logo-32.png" ]  && cp "$PROJECT_DIR/assets/logo/tinexus-logo-32.png"  "$ROOTFS_DIR/usr/share/icons/hicolor/32x32/apps/tinexus-logo.png"
    [ -f "$PROJECT_DIR/assets/logo/tinexus-logo-24.png" ]  && cp "$PROJECT_DIR/assets/logo/tinexus-logo-24.png"  "$ROOTFS_DIR/usr/share/icons/hicolor/24x24/apps/tinexus-logo.png"
    [ -f "$PROJECT_DIR/assets/logo/tinexus-logo-16.png" ]  && cp "$PROJECT_DIR/assets/logo/tinexus-logo-16.png"  "$ROOTFS_DIR/usr/share/icons/hicolor/16x16/apps/tinexus-logo.png"
    success "Staged Tinexus brand logo and hicolor icons."


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

    # Stage custom wallpaper image from Temp directory or assets/
    info "Staging custom wallpaper image into rootfs..."
    mkdir -p "$ROOTFS_DIR/usr/share/backgrounds"
    mkdir -p "$ROOTFS_DIR/home/tinexus/Pictures"
    mkdir -p "$ROOTFS_DIR/home/tinexus/Desktop"
    if [ -d "$PROJECT_DIR/assets/wallpaper" ]; then
        cp -L "$PROJECT_DIR"/assets/wallpaper/* "$ROOTFS_DIR/usr/share/backgrounds/" 2>/dev/null || true
        cp -L "$PROJECT_DIR"/assets/wallpaper/* "$ROOTFS_DIR/home/tinexus/Pictures/" 2>/dev/null || true
        success "Staged wallpapers & test images to /usr/share/backgrounds/ and ~/Pictures/."
    fi
    if [ -f "$PROJECT_DIR/Temp/tinexus-default.jpg" ]; then
        cp -L "$PROJECT_DIR/Temp/tinexus-default.jpg" "$ROOTFS_DIR/usr/share/backgrounds/tinexus-default.jpg"
        cp -L "$PROJECT_DIR/Temp/tinexus-default.jpg" "$ROOTFS_DIR/home/tinexus/Pictures/tinexus-default.jpg"
    fi

    # Stage timezone data so localtime_r() returns correct local time
    info "Staging timezone data (Asia/Kolkata)..."
    if [ -f "/usr/share/zoneinfo/Asia/Kolkata" ]; then
        mkdir -p "$ROOTFS_DIR/usr/share/zoneinfo/Asia"
        cp /usr/share/zoneinfo/Asia/Kolkata "$ROOTFS_DIR/usr/share/zoneinfo/Asia/Kolkata"
        ln -sf /usr/share/zoneinfo/Asia/Kolkata "$ROOTFS_DIR/etc/localtime"
        echo "Asia/Kolkata" > "$ROOTFS_DIR/etc/timezone"
        success "Timezone set to Asia/Kolkata (IST UTC+5:30)."
    else
        warn "Zoneinfo not found on build host — time will show UTC."
    fi

    # Stage any AppImages provided by the user in Temp/
    info "Checking for 3rd-party AppImages in Temp/..."
    if ls "$PROJECT_DIR/Temp/"*.AppImage 1> /dev/null 2>&1; then
        mkdir -p "$ROOTFS_DIR/opt/AppImages"
        cp -L "$PROJECT_DIR/Temp/"*.AppImage "$ROOTFS_DIR/opt/AppImages/" 2>/dev/null || true
        chmod +x "$ROOTFS_DIR/opt/AppImages/"*.AppImage 2>/dev/null || true
        success "Staged user-provided AppImages into /opt/AppImages/"

        # Ensure secure permissions for Tinexus Apps and Trust Overrides directories
        mkdir -p "$ROOTFS_DIR/opt/tinexus-apps"
        mkdir -p "$ROOTFS_DIR/var/lib/tinexus"
        # Note: Since fakeroot/iso builder runs as root during squashfs, these are implicitly root-owned
        # but we enforce the directory modes explicitly.
        chmod 0755 "$ROOTFS_DIR/opt/tinexus-apps"
        chmod 0755 "$ROOTFS_DIR/var/lib/tinexus"

        # [Jugaad] Stage full desktop libraries required by heavy AppImages like Chrome
        info "[Jugaad] Staging Chrome/AppImage shared library dependencies (GTK, NSS, X11)..."
        CHROME_LIBS=(
            "libglib-2.0.so.0" "libgobject-2.0.so.0" "libnspr4.so" "libnss3.so" "libnssutil3.so"
            "libsmime3.so" "libgio-2.0.so.0" "libatk-1.0.so.0" "libatk-bridge-2.0.so.0" "libdbus-1.so.3"
            "libcups.so.2" "libexpat.so.1" "libfontconfig.so.1" "libX11.so.6" "libxcb.so.1"
            "libxkbcommon.so.0" "libasound.so.2" "libgbm.so.1" "libXext.so.6" "libcairo.so.2"
            "libpango-1.0.so.0" "libudev.so.1" "libXcomposite.so.1" "libXdamage.so.1" "libXfixes.so.3"
            "libXrandr.so.2" "libatspi.so.0" "libm.so.6" "libgcc_s.so.1" "libc.so.6" "libatomic.so.1"
            "libpcre2-8.so.0" "libffi.so.8" "libplc4.so" "libplds4.so" "libgmodule-2.0.so.0" "libz.so.1"
            "libmount.so.1" "libselinux.so.1" "libsystemd.so.0" "libgssapi_krb5.so.2" "libavahi-common.so.3"
            "libavahi-client.so.3" "libgnutls.so.30" "libfreetype.so.6" "libXau.so.6" "libXdmcp.so.6"
            "libdrm.so.2" "libpng16.so.16" "libXrender.so.1" "libxcb-render.so.0" "libxcb-shm.so.0"
            "libpixman-1.so.0" "libfribidi.so.0" "libthai.so.0" "libharfbuzz.so.0" "libXi.so.6"
            "libXRes.so.1" "libblkid.so.1" "libkrb5.so.3" "libk5crypto.so.3" "libcom_err.so.2"
            "libkrb5support.so.0" "libp11-kit.so.0" "libidn2.so.0" "libunistring.so.5" "libtasn1.so.6"
            "libhogweed.so.6" "libnettle.so.8" "libgmp.so.10" "libbz2.so.1.0" "libbrotlidec.so.1"
            "libdatrie.so.1" "libgraphite2.so.3" "libkeyutils.so.1" "libresolv.so.2" "libbrotlicommon.so.1"
            "libwayland-client.so.0" "libwayland-cursor.so.0" "libwayland-egl.so.1" "libnssckbi.so"
        )
        for lib in "${CHROME_LIBS[@]}"; do
            for search_dir in /usr/lib/x86_64-linux-gnu /lib/x86_64-linux-gnu /usr/lib /lib; do
                if [ -f "$search_dir/$lib" ]; then
                    mkdir -p "$ROOTFS_DIR$search_dir"
                    cp -L "$search_dir/$lib" "$ROOTFS_DIR$search_dir/" 2>/dev/null || true
                    break
                fi
            done
        done
        success "Staged 70+ Chrome library dependencies."
    fi

    # Locale — foot uses LC_ALL/LANG; stage minimal C.UTF-8
    info "Staging locale data (C.UTF-8)..."
    mkdir -p "$ROOTFS_DIR/usr/share/locale"
    mkdir -p "$ROOTFS_DIR/usr/lib/locale"
    if [ -f "/usr/lib/locale/locale-archive" ]; then
        cp -L "/usr/lib/locale/locale-archive" "$ROOTFS_DIR/usr/lib/locale/" 2>/dev/null || true
    fi
    if [ -d "/usr/lib/locale/C.utf8" ]; then
        cp -r "/usr/lib/locale/C.utf8" "$ROOTFS_DIR/usr/lib/locale/" 2>/dev/null || true
    elif [ -d "/usr/lib/locale/C.UTF-8" ]; then
        cp -r "/usr/lib/locale/C.UTF-8" "$ROOTFS_DIR/usr/lib/locale/" 2>/dev/null || true
    fi
    mkdir -p "$ROOTFS_DIR/etc"
    echo "LANG=C.UTF-8" > "$ROOTFS_DIR/etc/locale.conf"

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
    # We previously generated custom /etc/passwd, /etc/group, and /etc/shadow.
    # Do NOT copy the host's files, as they will overwrite the custom ones!

    # ── Non-blocking Userspace Networking Staging (DHCP / DNS) ────────────────
    # POLICY BOUNDARY: Core platform daemons (comp/searchd/serviced/ipcd) never touch
    # network connections. DHCP/DNS staging is purely for user-space apps (browser, curl, AppImages).
    info "Staging lightweight userspace network stack (udhcpc / DNS)..."
    mkdir -p "$ROOTFS_DIR/usr/share/udhcpc" "$ROOTFS_DIR/etc"
    cat > "$ROOTFS_DIR/usr/share/udhcpc/default.script" << 'EOF_DHCP'
#!/bin/sh
# udhcpc script for Tinexus OS (Purely for user-space client application networking)
[ -z "$1" ] && exit 1
RESOLV_CONF="/etc/resolv.conf"

case "$1" in
    deconfig)
        /bin/ip addr flush dev "$interface" 2>/dev/null || /sbin/ifconfig "$interface" 0.0.0.0 2>/dev/null || true
        ;;
    renew|bound)
        if [ -n "$ip" ]; then
            /bin/ip addr add "$ip/$mask" dev "$interface" 2>/dev/null || /sbin/ifconfig "$interface" "$ip" netmask "$subnet" 2>/dev/null || true
        fi
        if [ -n "$router" ]; then
            for r in $router; do
                /bin/ip route add default via "$r" dev "$interface" 2>/dev/null || /sbin/route add default gw "$r" dev "$interface" 2>/dev/null || true
                break
            done
        fi
        if [ -n "$dns" ]; then
            echo -n > "$RESOLV_CONF.tmp"
            [ -n "$domain" ] && echo "search $domain" >> "$RESOLV_CONF.tmp"
            for d in $dns; do
                echo "nameserver $d" >> "$RESOLV_CONF.tmp"
            done
            mv -f "$RESOLV_CONF.tmp" "$RESOLV_CONF"
        fi
        ;;
esac
exit 0
EOF_DHCP
    chmod 0755 "$ROOTFS_DIR/usr/share/udhcpc/default.script"

    # Default fallback DNS nameserver
    if [ ! -f "$ROOTFS_DIR/etc/resolv.conf" ]; then
        echo "nameserver 1.1.1.1" > "$ROOTFS_DIR/etc/resolv.conf"
        echo "nameserver 8.8.8.8" >> "$ROOTFS_DIR/etc/resolv.conf"
    fi

    # Ensure busybox networking applets are symlinked
    local bb_host="$(command -v busybox || true)"
    if [ -n "$bb_host" ] && [ -f "$bb_host" ]; then
        cp -L "$bb_host" "$ROOTFS_DIR/bin/busybox"
        for net_app in udhcpc ip ifconfig route ping wget; do
            ln -sf busybox "$ROOTFS_DIR/bin/$net_app" 2>/dev/null || true
            ln -sf /bin/busybox "$ROOTFS_DIR/sbin/$net_app" 2>/dev/null || true
            ln -sf /bin/busybox "$ROOTFS_DIR/usr/bin/$net_app" 2>/dev/null || true
        done
        success "Staged busybox networking tools (udhcpc, ip, ifconfig, route, ping, wget)."
    fi

    # ── Stage Intel iGPU Firmware (Comet Lake / Kaby Lake / Skylake) ─────────
    info "Staging Intel iGPU firmware for Comet Lake-H (ASUS TUF F15)..."
    mkdir -p "$ROOTFS_DIR/lib/firmware/i915"
    if [ -d "/lib/firmware/i915" ]; then
        for fw in /lib/firmware/i915/cml* /lib/firmware/i915/kbl* /lib/firmware/i915/skl*; do
            [ -e "$fw" ] || continue
            cp -L "$fw" "$ROOTFS_DIR/lib/firmware/i915/" 2>/dev/null || true
        done
        for compressed in "$ROOTFS_DIR/lib/firmware/i915"/*.zst; do
            [ -f "$compressed" ] && zstd -d --keep "$compressed" 2>/dev/null || true
        done
        success "Staged $(ls "$ROOTFS_DIR/lib/firmware/i915" | wc -l) Intel i915 firmware files into rootfs."
    else
        warn "/lib/firmware/i915 not found on host. Intel iGPU may fail to initialize without firmware."
    fi

    # ── Stage MediaTek Wi-Fi 6 MT7921 / MT7922 / MT7961 Firmware ────────────
    info "Staging MediaTek Wi-Fi 6 firmware (MT7921 / MT7922 / MT7961)..."
    mkdir -p "$ROOTFS_DIR/lib/firmware/mediatek"
    if [ -d "/lib/firmware/mediatek" ]; then
        for fw in /lib/firmware/mediatek/*MT7922* /lib/firmware/mediatek/*MT7961*; do
            [ -e "$fw" ] || continue
            cp -L "$fw" "$ROOTFS_DIR/lib/firmware/mediatek/" 2>/dev/null || true
        done
        for compressed in "$ROOTFS_DIR/lib/firmware/mediatek"/*.zst; do
            [ -f "$compressed" ] && zstd -d --keep "$compressed" 2>/dev/null || true
        done
        success "Staged $(ls "$ROOTFS_DIR/lib/firmware/mediatek" | wc -l) MediaTek firmware files into rootfs."
    else
        warn "/lib/firmware/mediatek not found on host."
    fi

    # ── Stage Wireless Utilities (wpa_supplicant, wpa_passphrase, wpa_cli, iw, rfkill) ──
    info "Staging wireless tools (wpa_supplicant, wpa_passphrase, wpa_cli, iw, rfkill)..."
    for tool in wpa_supplicant wpa_passphrase wpa_cli iw rfkill; do
        tool_path="$(command -v "$tool" || true)"
        if [ -n "$tool_path" ] && [ -f "$tool_path" ]; then
            cp -L "$tool_path" "$ROOTFS_DIR/usr/sbin/" 2>/dev/null || true
            cp -L "$tool_path" "$ROOTFS_DIR/usr/bin/" 2>/dev/null || true
            ldd "$tool_path" 2>/dev/null | sed -n 's/.*=> \(.*\) (0x.*/\1/p' | while read -r lib; do
                [ -f "$lib" ] && { mkdir -p "$ROOTFS_DIR$(dirname "$lib")"; cp -L "$lib" "$ROOTFS_DIR$lib" 2>/dev/null || true; }
            done
            ldd "$tool_path" 2>/dev/null | sed -n 's/^[[:space:]]*\(\/.*\) (0x.*/\1/p' | while read -r ld_loader; do
                [ -f "$ld_loader" ] && { mkdir -p "$ROOTFS_DIR$(dirname "$ld_loader")"; cp -L "$ld_loader" "$ROOTFS_DIR$ld_loader" 2>/dev/null || true; }
            done
        fi
    done
    mkdir -p "$ROOTFS_DIR/etc/wpa_supplicant" "$ROOTFS_DIR/var/run/wpa_supplicant" "$ROOTFS_DIR/usr/share/dbus-1/system-services" "$ROOTFS_DIR/usr/share/dbus-1/system.d" "$ROOTFS_DIR/etc/dbus-1/system.d"
    cat > "$ROOTFS_DIR/etc/wpa_supplicant/wpa_supplicant.conf" << 'EOF_WPA'
ctrl_interface=/var/run/wpa_supplicant
update_config=1
EOF_WPA
    [ -f "/usr/share/dbus-1/system-services/fi.w1.wpa_supplicant1.service" ] && cp -L "/usr/share/dbus-1/system-services/fi.w1.wpa_supplicant1.service" "$ROOTFS_DIR/usr/share/dbus-1/system-services/" 2>/dev/null || true
    [ -f "/usr/share/dbus-1/system.d/wpa_supplicant.conf" ] && cp -L "/usr/share/dbus-1/system.d/wpa_supplicant.conf" "$ROOTFS_DIR/usr/share/dbus-1/system.d/" 2>/dev/null || true
    [ -f "/usr/share/dbus-1/system.d/wpa_supplicant.conf" ] && cp -L "/usr/share/dbus-1/system.d/wpa_supplicant.conf" "$ROOTFS_DIR/etc/dbus-1/system.d/" 2>/dev/null || true

    # ── Hostname Configuration (Option 12 Registration on DHCP) ──
    echo "Tinexus-Desktop" > "$ROOTFS_DIR/etc/hostname"
    cat > "$ROOTFS_DIR/etc/hosts" << 'EOF_HOSTS'
127.0.0.1   localhost Tinexus-Desktop
::1         localhost ip6-localhost ip6-loopback
EOF_HOSTS

    cat > "$ROOTFS_DIR/usr/bin/tinexus-wifi" << 'EOF_WIFI'
#!/bin/sh
rfkill unblock all 2>/dev/null || true

# Dynamically discover primary Wi-Fi interface (wlo1, wlp3s0, wlan0, etc.)
IFACE=""
if [ -d /sys/class/net ]; then
    for d in /sys/class/net/*; do
        dev=$(basename "$d")
        [ "$dev" = "lo" ] && continue
        if [ -d "$d/wireless" ] || [ -d "$d/phy80211" ]; then
            IFACE="$dev"
            break
        fi
    done
fi
[ -z "$IFACE" ] && IFACE="wlan0"

ip link set "$IFACE" up 2>/dev/null || true
case "$1" in
    scan)
        echo "Scanning available Wi-Fi networks on $IFACE..."
        iw dev "$IFACE" scan 2>/dev/null | grep -E "SSID: " | sed 's/^[ \t]*SSID: //' | sort -u
        ;;
    connect)
        SSID="$2"
        PASS="$3"
        if [ -z "$SSID" ]; then
            echo "Usage: tinexus-wifi connect <SSID> [password]"
            exit 1
        fi
        killall wpa_supplicant 2>/dev/null || true
        CONF="/tmp/wpa_connect.conf"
        if [ -n "$PASS" ]; then
            wpa_passphrase "$SSID" "$PASS" > "$CONF" 2>/dev/null
        else
            cat > "$CONF" << EOF_OPEN
network={
    ssid="$SSID"
    key_mgmt=NONE
}
EOF_OPEN
        fi
        echo "Connecting to '$SSID' on interface '$IFACE'..."
        wpa_supplicant -B -i "$IFACE" -c "$CONF"
        sleep 2
        echo "Obtaining IP address via DHCP with hostname 'Tinexus-Desktop'..."
        udhcpc -i "$IFACE" -s /usr/share/udhcpc/default.script -x hostname:Tinexus-Desktop -q -n
        echo "Testing internet connectivity..."
        ping -c 3 -W 3 1.1.1.1 || ping -c 3 -W 3 8.8.8.8
        ;;
    status)
        echo "Interface: $IFACE"
        ip addr show "$IFACE"
        iw dev "$IFACE" link
        ;;
    *)
        echo "Tinexus OS Wi-Fi Manager (Active Interface: $IFACE)"
        echo "Usage: tinexus-wifi scan"
        echo "       tinexus-wifi connect <SSID> [password]"
        echo "       tinexus-wifi status"
        ;;
esac
EOF_WIFI
    chmod 0755 "$ROOTFS_DIR/usr/bin/tinexus-wifi"

    info "Staging real hardware kernel modules (i915, NVMe, USB, Ethernet, Touchpad, Wi-Fi, QEMU) into rootfs..."
    mkdir -p "$ROOTFS_DIR/lib/modules/7.0.0-28-generic" "$ROOTFS_DIR/lib/modules"
    if [ -d "/lib/modules/7.0.0-28-generic" ]; then
        find "/lib/modules/7.0.0-28-generic" -type f \( \
            -name "isofs.ko*" -o -name "ahci.ko*" -o -name "libahci.ko*" \
            -o -name "nvme.ko*" -o -name "nvme-core.ko*" -o -name "nvme-auth.ko*" -o -name "nvme-keyring.ko*" -o -name "hkdf.ko*" \
            -o -name "uas.ko*" -o -name "usb-storage.ko*" \
            -o -name "overlay.ko*" \
            -o -name "i915.ko*" -o -name "drm_display_helper.ko*" -o -name "ttm.ko*" -o -name "video.ko*" \
            -o -name "drm_buddy.ko*" -o -name "cec.ko*" -o -name "rc-core.ko*" -o -name "i2c-algo-bit.ko*" -o -name "wmi.ko*" -o -name "intel-gtt.ko*" \
            -o -name "intel-lpss.ko*" -o -name "intel-lpss-pci.ko*" \
            -o -name "i2c-designware-core.ko*" -o -name "i2c-designware-pci.ko*" -o -name "i2c-ccgx-ucsi.ko*" \
            -o -name "i2c-hid.ko*" -o -name "i2c-hid-acpi.ko*" -o -name "hid-multitouch.ko*" \
            -o -name "pinctrl-cannonlake.ko*" \
            -o -name "mt7921e.ko*" -o -name "mt7921-common.ko*" -o -name "mt792x-lib.ko*" -o -name "mt76-connac-lib.ko*" -o -name "mt76.ko*" \
            -o -name "mac80211.ko*" -o -name "cfg80211.ko*" -o -name "libarc4.ko*" -o -name "rfkill.ko*" \
            -o -name "virtio-gpu.ko*" -o -name "virtio_dma_buf.ko*" -o -name "bochs.ko*" \
            -o -name "virtio_input.ko*" \
            -o -name "virtio_net.ko*" -o -name "net_failover.ko*" -o -name "failover.ko*" \
            -o -name "e1000.ko*" -o -name "e1000e.ko*" -o -name "r8169.ko*" -o -name "igb.ko*" -o -name "tg3.ko*" \
            -o -name "evdev.ko*" \
            -o -name "hid.ko*" -o -name "hid-generic.ko*" -o -name "usbhid.ko*" \
        \) | while read -r mod; do
            cp -L "$mod" "$ROOTFS_DIR/lib/modules/7.0.0-28-generic/"
            cp -L "$mod" "$ROOTFS_DIR/lib/modules/"
        done
        for compressed in "$ROOTFS_DIR/lib/modules/7.0.0-28-generic"/*.zst "$ROOTFS_DIR/lib/modules"/*.zst; do
            [ -f "$compressed" ] && zstd -d --rm "$compressed" 2>/dev/null || true
        done
        # Copy modules.order and modules.builtin so depmod can resolve symbols accurately
        cp /lib/modules/7.0.0-28-generic/modules.order "$ROOTFS_DIR/lib/modules/7.0.0-28-generic/" 2>/dev/null || true
        cp /lib/modules/7.0.0-28-generic/modules.builtin* "$ROOTFS_DIR/lib/modules/7.0.0-28-generic/" 2>/dev/null || true
        if [ -f "/usr/sbin/depmod" ]; then
            /usr/sbin/depmod -b "$ROOTFS_DIR" 7.0.0-28-generic 2>/dev/null || true
        fi
    fi

    if [ -f "$ROOTFS_DIR/usr/bin/tinexus-serviced" ]; then
        ln -sf /usr/bin/tinexus-serviced "$ROOTFS_DIR/sbin/init"
    else
        warn "tinexus-serviced not found in rootfs! System may not boot properly."
    fi

    # Ensure dynamic linker cache is populated for all staged binaries and libraries
    mkdir -p "$ROOTFS_DIR/etc"
    cat > "$ROOTFS_DIR/etc/ld.so.conf" << 'EOF_LD'
/lib
/usr/lib
/lib/x86_64-linux-gnu
/usr/lib/x86_64-linux-gnu
/usr/local/lib
EOF_LD
    if command -v ldconfig >/dev/null 2>&1; then
        ldconfig -r "$ROOTFS_DIR" 2>/dev/null || true
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
    # The splash binary is built to build/bin/ per its CMakeLists RUNTIME_OUTPUT_DIRECTORY
    SPLASH_BIN=""
    for candidate in "$BUILD_DIR/bin/tinexus-splash" "$BUILD_DIR/tinexus-splash" "$BUILD_DIR/src/splash/tinexus-splash"; do
        if [ -f "$candidate" ]; then SPLASH_BIN="$candidate"; break; fi
    done
    if [ -n "$SPLASH_BIN" ]; then
        cp -L "$SPLASH_BIN" "$init_staging/bin/tinexus-splash"
        (ldd "$SPLASH_BIN" 2>/dev/null || true) | sed -n 's/.*=> \(.*\) (0x.*/\1/p' | while read -r lib; do
            [ -f "$lib" ] && { mkdir -p "$init_staging$(dirname "$lib")"; cp -L "$lib" "$init_staging$lib" 2>/dev/null || true; }
        done
        (ldd "$SPLASH_BIN" 2>/dev/null || true) | sed -n 's/^[[:space:]]*\(\/.*\) (0x.*/\1/p' | while read -r ld_loader; do
            [ -f "$ld_loader" ] && { mkdir -p "$init_staging$(dirname "$ld_loader")"; cp -L "$ld_loader" "$init_staging$ld_loader" 2>/dev/null || true; }
        done
        success "tinexus-splash staged from $SPLASH_BIN"
    else
        warn "tinexus-splash binary not found!"
    fi
    if [ -f "$PROJECT_DIR/Temp/tinexus-logo.png" ]; then
        cp -L "$PROJECT_DIR/Temp/tinexus-logo.png" "$init_staging/"
    fi

    local bb_bin="$(command -v busybox || command -v sh || echo /bin/sh)"
    cp -L "$bb_bin" "$init_staging/bin/busybox"
    for cmd in sh cat ls mkdir mount umount mdev switch_root sleep chroot grep dmesg clear head tail uname df; do 
        ln -sf busybox "$init_staging/bin/$cmd" || true
    done

    (ldd "$bb_bin" 2>/dev/null || true) | sed -n 's/.*=> \(.*\) (0x.*/\1/p' | while read -r lib; do
        [ -f "$lib" ] && { mkdir -p "$init_staging$(dirname "$lib")"; cp -L "$lib" "$init_staging$lib" 2>/dev/null || true; }
    done
    (ldd "$bb_bin" 2>/dev/null || true) | sed -n 's/^[[:space:]]*\(\/.*\) (0x.*/\1/p' | while read -r ld_loader; do
        [ -f "$ld_loader" ] && { mkdir -p "$init_staging$(dirname "$ld_loader")"; cp -L "$ld_loader" "$init_staging$ld_loader" 2>/dev/null || true; }
    done

    # Stage blkid for dynamic filesystem label detection
    if [ -f "/usr/sbin/blkid" ]; then
        cp -L "/usr/sbin/blkid" "$init_staging/bin/"
        ldd "/usr/sbin/blkid" 2>/dev/null | sed -n 's/.*=> \(.*\) (0x.*/\1/p' | while read -r lib; do
            [ -f "$lib" ] && { mkdir -p "$init_staging$(dirname "$lib")"; cp -L "$lib" "$init_staging$lib" 2>/dev/null || true; }
        done
        ldd "/usr/sbin/blkid" 2>/dev/null | sed -n 's/^[[:space:]]*\(\/.*\) (0x.*/\1/p' | while read -r ld_loader; do
            [ -f "$ld_loader" ] && { mkdir -p "$init_staging$(dirname "$ld_loader")"; cp -L "$ld_loader" "$init_staging$ld_loader" 2>/dev/null || true; }
        done
    fi

    # Stage kmod/modprobe for automatic module dependency resolution
    if [ -f "/usr/bin/kmod" ]; then
        cp -L "/usr/bin/kmod" "$init_staging/bin/"
        ln -sf kmod "$init_staging/bin/modprobe"
        ln -sf kmod "$init_staging/bin/depmod"
        ldd "/usr/bin/kmod" 2>/dev/null | sed -n 's/.*=> \(.*\) (0x.*/\1/p' | while read -r lib; do
            [ -f "$lib" ] && { mkdir -p "$init_staging$(dirname "$lib")"; cp -L "$lib" "$init_staging$lib" 2>/dev/null || true; }
        done
        ldd "/usr/bin/kmod" 2>/dev/null | sed -n 's/^[[:space:]]*\(\/.*\) (0x.*/\1/p' | while read -r ld_loader; do
            [ -f "$ld_loader" ] && { mkdir -p "$init_staging$(dirname "$ld_loader")"; cp -L "$ld_loader" "$init_staging$ld_loader" 2>/dev/null || true; }
        done
    fi

    # Stage Intel iGPU firmware into initramfs (Comet Lake-H GuC/HuC/DMC)
    mkdir -p "$init_staging/lib/firmware/i915"
    if [ -d "/lib/firmware/i915" ]; then
        for fw in /lib/firmware/i915/cml* /lib/firmware/i915/kbl* /lib/firmware/i915/skl*; do
            [ -e "$fw" ] || continue
            cp -L "$fw" "$init_staging/lib/firmware/i915/" 2>/dev/null || true
        done
        for compressed in "$init_staging/lib/firmware/i915"/*.zst; do
            [ -f "$compressed" ] && zstd -d --keep "$compressed" 2>/dev/null || true
        done
        success "Staged $(ls "$init_staging/lib/firmware/i915" | wc -l) Intel i915 firmware files into initramfs."
    fi

    # Stage MediaTek Wi-Fi 6 firmware into initramfs (MT7921 / MT7922 / MT7961)
    mkdir -p "$init_staging/lib/firmware/mediatek"
    if [ -d "/lib/firmware/mediatek" ]; then
        for fw in /lib/firmware/mediatek/*MT7922* /lib/firmware/mediatek/*MT7961*; do
            [ -e "$fw" ] || continue
            cp -L "$fw" "$init_staging/lib/firmware/mediatek/" 2>/dev/null || true
        done
        for compressed in "$init_staging/lib/firmware/mediatek"/*.zst; do
            [ -f "$compressed" ] && zstd -d --keep "$compressed" 2>/dev/null || true
        done
        success "Staged $(ls "$init_staging/lib/firmware/mediatek" | wc -l) MediaTek firmware files into initramfs."
    fi

    mkdir -p "$init_staging/lib/modules/7.0.0-28-generic" "$init_staging/lib/modules"
    local kver="7.0.0-28-generic"
    if [ -d "/lib/modules/$kver" ]; then
        find "/lib/modules/$kver" -type f \( \
            -name "isofs.ko*" -o -name "ahci.ko*" -o -name "libahci.ko*" \
            -o -name "nvme.ko*" -o -name "nvme-core.ko*" -o -name "nvme-auth.ko*" -o -name "nvme-keyring.ko*" -o -name "hkdf.ko*" \
            -o -name "uas.ko*" -o -name "usb-storage.ko*" \
            -o -name "overlay.ko*" \
            -o -name "i915.ko*" -o -name "drm_display_helper.ko*" -o -name "ttm.ko*" -o -name "video.ko*" \
            -o -name "drm_buddy.ko*" -o -name "cec.ko*" -o -name "rc-core.ko*" -o -name "i2c-algo-bit.ko*" -o -name "wmi.ko*" -o -name "intel-gtt.ko*" \
            -o -name "intel-lpss.ko*" -o -name "intel-lpss-pci.ko*" \
            -o -name "i2c-designware-core.ko*" -o -name "i2c-designware-pci.ko*" -o -name "i2c-ccgx-ucsi.ko*" \
            -o -name "i2c-hid.ko*" -o -name "i2c-hid-acpi.ko*" -o -name "hid-multitouch.ko*" \
            -o -name "pinctrl-cannonlake.ko*" \
            -o -name "mt7921e.ko*" -o -name "mt7921-common.ko*" -o -name "mt792x-lib.ko*" -o -name "mt76-connac-lib.ko*" -o -name "mt76.ko*" \
            -o -name "mac80211.ko*" -o -name "cfg80211.ko*" -o -name "libarc4.ko*" -o -name "rfkill.ko*" \
            -o -name "virtio-gpu.ko*" -o -name "virtio_dma_buf.ko*" -o -name "bochs.ko*" \
            -o -name "virtio_input.ko*" \
            -o -name "evdev.ko*" \
            -o -name "hid.ko*" -o -name "hid-generic.ko*" -o -name "usbhid.ko*" \
        \) | while read -r mod; do
            cp -L "$mod" "$init_staging/lib/modules/7.0.0-28-generic/"
            cp -L "$mod" "$init_staging/lib/modules/"
        done
        for compressed in "$init_staging/lib/modules/7.0.0-28-generic"/*.zst "$init_staging/lib/modules"/*.zst; do
            [ -f "$compressed" ] && zstd -d --rm "$compressed" 2>/dev/null || true
        done
        cp /lib/modules/7.0.0-28-generic/modules.order "$init_staging/lib/modules/7.0.0-28-generic/" 2>/dev/null || true
        cp /lib/modules/7.0.0-28-generic/modules.builtin* "$init_staging/lib/modules/7.0.0-28-generic/" 2>/dev/null || true
        if [ -f "/usr/sbin/depmod" ]; then
            /usr/sbin/depmod -b "$init_staging" 7.0.0-28-generic 2>/dev/null || true
        fi
    fi

    cat > "$init_staging/init" << 'EOINIT'
#!/bin/sh
# ── Tinexus OS Early Boot Initramfs Script ─────────────────────────────────────
# Mount essential virtual filesystems
/bin/mount -t proc proc /proc 2>/dev/null
/bin/mount -t sysfs sysfs /sys 2>/dev/null
/bin/mount -t devtmpfs devtmpfs /dev 2>/dev/null || /bin/mdev -s 2>/dev/null

# Re-open stdin, stdout, stderr on /dev/console so that all user-space output appears on the laptop screen
if [ -c /dev/console ]; then
    exec </dev/console >/dev/console 2>&1
fi

log_step() {
    echo ">>> [TINEXUS-STEP] $1"
    echo "<6>>>> [TINEXUS-STEP] $1" > /dev/kmsg 2>/dev/null || true
    echo ">>> [TINEXUS-STEP] $1" > /dev/console 2>/dev/null || true
    [ -c /dev/tty0 ] && echo ">>> [TINEXUS-STEP] $1" > /dev/tty0 2>/dev/null || true
}

log_step "STEP 1: Initramfs mounted /proc, /sys, /dev successfully."

# Check if debug mode was requested on the kernel command line
IS_DEBUG=0
if [ -f /proc/cmdline ]; then
    CMDLINE=$(cat /proc/cmdline)
    case "$CMDLINE" in
        *tinexus.debug*|*rd.break*|*break*|*single*)
            IS_DEBUG=1
            ;;
    esac
fi

if [ "$IS_DEBUG" = "1" ]; then
    log_step "DEBUG MODE ACTIVE (detected in cmdline: $CMDLINE)"
fi

log_step "STEP 2: Loading hardware drivers in strict dependency order..."

load_mod() {
    local m="$1"
    # Try modprobe first
    modprobe -d / "$m" 2>/dev/null
    if [ $? -eq 0 ]; then
        return 0
    fi
    # Direct insmod fallback
    for p in /lib/modules/7.0.0-28-generic /lib/modules; do
        if [ -f "$p/$m.ko" ]; then
            insmod "$p/$m.ko" 2>/dev/null && return 0
        fi
    done
    return 1
}

# 1. Storage & bus controllers (with sub-dependencies)
for m in libahci ahci isofs hkdf nvme-keyring nvme-auth nvme-core nvme usb-storage uas overlay; do
    load_mod "$m" || true
done

# 2. Input devices & Touchpad (ASUS ELAN1203 on Intel Serial IO I2C Host 06E8)
for m in hid hid-generic usbhid evdev intel-lpss intel-lpss-pci pinctrl-cannonlake i2c-ccgx-ucsi i2c-designware-pci i2c-hid i2c-hid-acpi hid-multitouch; do
    load_mod "$m" || true
done

# 3. Graphics display: strict topological dependency order for Intel i915
# rc-core -> cec -> drm_display_helper
# wmi -> video
# ttm, drm_buddy, i2c-algo-bit -> i915
for m in rc-core cec drm_display_helper wmi video ttm drm_buddy i2c-algo-bit i915; do
    load_mod "$m" || true
done

# 4. Wireless network (MediaTek Wi-Fi 6 MT7921)
# libarc4 -> rfkill -> cfg80211 -> mac80211 -> mt76 -> mt76-connac-lib -> mt792x-lib -> mt7921-common -> mt7921e
for m in libarc4 rfkill cfg80211 mac80211 mt76 mt76-connac-lib mt792x-lib mt7921-common mt7921e; do
    load_mod "$m" || true
done
[ -x /bin/rfkill ] && rfkill unblock all 2>/dev/null || true

# 5. Virtualization / fallback graphics drivers
for m in virtio_dma_buf virtio-gpu virtio_input bochs; do
    load_mod "$m" || true
done

/bin/mdev -s 2>/dev/null

log_step "STEP 3: Hardware drivers loaded. Verifying DRI / DRM subsystem..."
if [ -d /dev/dri ]; then
    log_step "Found /dev/dri directory with nodes:"
    for node in /dev/dri/*; do
        [ -e "$node" ] && log_step "  - $node"
    done
else
    log_step "WARNING: /dev/dri directory not found after driver load!"
fi

if [ -d /sys/class/drm ]; then
    log_step "Found /sys/class/drm entries:"
    for conn in /sys/class/drm/*; do
        [ -e "$conn" ] || continue
        cname=$(basename "$conn")
        if [ -f "$conn/status" ]; then
            cstat=$(cat "$conn/status" 2>/dev/null || echo "unknown")
            log_step "  - $cname (status: $cstat)"
        else
            log_step "  - $cname"
        fi
    done
else
    log_step "WARNING: /sys/class/drm not found!"
fi

log_step "STEP 4: Scanning for Live CD / USB media..."

# Graphics modules loaded, start splash screen if present (skip in debug mode)
if [ "$IS_DEBUG" != "1" ] && [ -x /bin/tinexus-splash ]; then
    /bin/tinexus-splash &
    sleep 0.1
    echo 20 > /tmp/splash_progress 2>/dev/null
fi

ROOT_DEV=""
for attempt in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
    echo $((20 + attempt * 3)) > /tmp/splash_progress 2>/dev/null
    /bin/mdev -s 2>/dev/null

    # 1. Search by filesystem label TINEXUS_LIVE via blkid
    if [ -x /bin/blkid ]; then
        BY_LABEL=$(/bin/blkid -L TINEXUS_LIVE 2>/dev/null || true)
        if [ -n "$BY_LABEL" ] && [ -b "$BY_LABEL" ]; then
            /bin/mount -o ro "$BY_LABEL" /mnt 2>/dev/null
            if [ -f /mnt/live/rootfs.squashfs ]; then
                log_step "Found rootfs via label on $BY_LABEL (attempt $attempt)"
                ROOT_DEV="$BY_LABEL"
                break
            fi
            /bin/umount /mnt 2>/dev/null
        fi
    fi

    # 2. Check /dev/disk/by-label/TINEXUS_LIVE
    if [ -b /dev/disk/by-label/TINEXUS_LIVE ]; then
        /bin/mount -o ro /dev/disk/by-label/TINEXUS_LIVE /mnt 2>/dev/null
        if [ -f /mnt/live/rootfs.squashfs ]; then
            log_step "Found rootfs on /dev/disk/by-label/TINEXUS_LIVE"
            ROOT_DEV="/dev/disk/by-label/TINEXUS_LIVE"
            break
        fi
        /bin/umount /mnt 2>/dev/null
    fi

    # 3. Fully dynamic scan across all block devices in /sys/block/
    for b in /sys/block/*; do
        [ -e "$b" ] || continue
        devname=$(basename "$b")
        case "$devname" in
            loop*|ram*|zram*) continue ;;
        esac
        for candidate in "/dev/$devname" "/dev/${devname}"p* "/dev/${devname}"[0-9]*; do
            [ -b "$candidate" ] || continue
            /bin/mount -o ro "$candidate" /mnt 2>/dev/null
            if [ -f /mnt/live/rootfs.squashfs ]; then
                log_step "Found rootfs via dynamic scan on $candidate (attempt $attempt)"
                ROOT_DEV="$candidate"
                break 3
            fi
            /bin/umount /mnt 2>/dev/null
        done
    done

    sleep 1
done

if [ -z "$ROOT_DEV" ] || [ ! -f /mnt/live/rootfs.squashfs ]; then
    log_step "FATAL: rootfs.squashfs not found on any storage device after 15s!"
    log_step "Dropping to emergency recovery shell..."
    exec /bin/sh </dev/console >/dev/console 2>&1
fi

log_step "STEP 5: Storage media located on $ROOT_DEV."
mkdir -p /newroot

log_step "STEP 6: Setting up writable OverlayFS (RAM tmpfs + SquashFS lowerdir)..."
mkdir -p /rofs /cow /newroot
/bin/mount -t squashfs -o ro /mnt/live/rootfs.squashfs /rofs
MNT_STATUS=$?
if [ $MNT_STATUS -ne 0 ]; then
    log_step "FATAL: mount -t squashfs failed with exit code $MNT_STATUS!"
    exec /bin/sh </dev/console >/dev/console 2>&1
fi

# Mount dynamic tmpfs for copy-on-write upper and work directories (up to 4GB dynamic RAM ceiling)
/bin/mount -t tmpfs -o size=4G,mode=0755 tmpfs /cow
mkdir -p /cow/upper /cow/work

# Mount OverlayFS uniting read-only SquashFS and writable tmpfs into /newroot
/bin/mount -t overlay overlay -o lowerdir=/rofs,upperdir=/cow/upper,workdir=/cow/work /newroot
OVERLAY_STATUS=$?
if [ $OVERLAY_STATUS -ne 0 ]; then
    log_step "WARNING: OverlayFS mount failed ($OVERLAY_STATUS)! Falling back to direct SquashFS read-only mount..."
    /bin/mount -t squashfs -o ro /mnt/live/rootfs.squashfs /newroot
else
    log_step "STEP 7: Writable OverlayFS mounted successfully on /newroot."
    df -h /newroot > /dev/console 2>&1 || true
fi

log_step "STEP 8: Mounting virtual filesystems into /newroot..."
/bin/mount -t devtmpfs devtmpfs /newroot/dev 2>/dev/null || true
mkdir -p /newroot/dev/shm /newroot/dev/pts
/bin/mount -t tmpfs tmpfs /newroot/dev/shm 2>/dev/null || true
/bin/mount -t devpts devpts /newroot/dev/pts 2>/dev/null || true
/bin/mount -t proc proc /newroot/proc 2>/dev/null || true
/bin/mount -t sysfs sysfs /newroot/sys 2>/dev/null || true
/bin/mount -t tmpfs tmpfs /newroot/run 2>/dev/null || true
/bin/mount -t tmpfs tmpfs /newroot/tmp 2>/dev/null || true

log_step "STEP 9: Setting up live session directories (/home/tinexus, /root)..."
mkdir -p /newroot/home/tinexus/Desktop /newroot/home/tinexus/Pictures /newroot/home/tinexus/.config
chown -R 1000:1000 /newroot/home/tinexus 2>/dev/null || true
mkdir -p /newroot/root

log_step "STEP 10: Verifying /newroot/usr/bin/tinexus-serviced and dynamic environment..."
if [ ! -f /newroot/usr/bin/tinexus-serviced ]; then
    log_step "FATAL: /newroot/usr/bin/tinexus-serviced DOES NOT EXIST!"
    ls -la /newroot/usr/bin/ > /dev/console 2>&1 || true
    exec /bin/sh </dev/console >/dev/console 2>&1
fi
if [ ! -x /newroot/usr/bin/tinexus-serviced ]; then
    log_step "WARNING: Setting chmod 0755 on /newroot/usr/bin/tinexus-serviced..."
    chmod 0755 /newroot/usr/bin/tinexus-serviced
fi

chroot_test=$(chroot /newroot /bin/sh -c "echo OK" 2>&1)
log_step "Chroot environment check into /newroot: $chroot_test"

# If debug was requested, drop to interactive shell before switch_root
if [ "$IS_DEBUG" = "1" ]; then
    log_step "================================================================"
    log_step "DEBUG SHELL ACTIVATED (triggered by kernel cmdline)"
    log_step "System state: SquashFS mounted at /newroot, USB at /mnt"
    log_step "Type 'exit' to resume boot and proceed to switch_root."
    log_step "================================================================"
    /bin/sh </dev/console >/dev/console 2>&1
    log_step "Debug shell exited. Resuming switch_root handoff..."
fi

log_step "STEP 11: ABOUT TO EXECUTE switch_root -c /dev/console /newroot /usr/bin/tinexus-serviced..."
echo 100 > /tmp/splash_progress 2>/dev/null
sleep 0.5

exec switch_root -c /dev/console /newroot /usr/bin/tinexus-serviced

# Fallback: if switch_root returns or fails
log_step "FATAL: switch_root failed or returned unexpectedly! Exit code: $?"
log_step "Dropping to emergency fallback shell on /dev/console..."
exec /bin/sh </dev/console >/dev/console 2>&1
EOINIT
    chmod 0755 "$init_staging/init"
    (cd "$init_staging" && find . | cpio -o -H newc 2>/dev/null | gzip -9 > "$INITRAMFS_OUT")
    success "Initramfs ready: $(du -sh "$INITRAMFS_OUT" | cut -f1)"

    cp "$VMLINUZ" "$ISO_TREE/boot/vmlinuz"
    success "Kernel staged: $(du -sh "$ISO_TREE/boot/vmlinuz" | cut -f1)"
}

# ── GRUB Configurations ───────────────────────────────────────────────────────
generate_grub_config() {
    mkdir -p "$ISO_TREE/boot/grub/fonts"
    
    # Stage unicode font for gfxterm graphical rendering
    local font_src=""
    for candidate in /usr/share/grub/unicode.pf2 /boot/grub/unicode.pf2 /boot/grub/fonts/unicode.pf2; do
        if [ -f "$candidate" ]; then
            font_src="$candidate"
            break
        fi
    done
    if [ -n "$font_src" ]; then
        cp -L "$font_src" "$ISO_TREE/boot/grub/fonts/unicode.pf2"
        mkdir -p "$ROOTFS_DIR/boot/grub/fonts"
        cp -L "$font_src" "$ROOTFS_DIR/boot/grub/fonts/unicode.pf2"
        info "Staged unicode.pf2 font ($font_src) for gfxterm."
    else
        warn "unicode.pf2 not found! GRUB gfxterm box borders may render as ???."
    fi

    cat > "$ISO_TREE/boot/grub/grub.cfg" << 'EOGRUB'
set default=0
set timeout=5

# Modern graphical high-definition display configuration
set gfxmode=1920x1080,1366x768,auto
set gfxpayload=keep
insmod all_video
insmod gfxterm
insmod gettext
insmod font

if loadfont ($root)/boot/grub/fonts/unicode.pf2 ; then
    terminal_output gfxterm
fi

# High-definition dark macOS-style palette
set menu_color_normal=light-gray/black
set menu_color_highlight=white/blue
set color_normal=light-gray/black
set color_highlight=white/blue

menuentry "Tinexus OS Live (Wayland Desktop)" {
    linux   /boot/vmlinuz root=live:CDLABEL=TINEXUS_LIVE boot=live rd.live.image rd.live.dir=/live rd.live.squashimg=rootfs.squashfs quiet loglevel=3 vt.global_cursor_default=0 logo.nologo fbcon=nodefer console=ttyS0,115200n8 console=tty0
    initrd  /boot/initramfs.img
}

menuentry "Tinexus OS Live (Safe Graphics / nomodeset)" {
    linux   /boot/vmlinuz root=live:CDLABEL=TINEXUS_LIVE boot=live rd.live.image rd.live.dir=/live rd.live.squashimg=rootfs.squashfs nomodeset console=ttyS0,115200n8 console=tty0
    initrd  /boot/initramfs.img
}

menuentry "Tinexus OS Live (Debug Console & Shell Break)" {
    linux   /boot/vmlinuz root=live:CDLABEL=TINEXUS_LIVE boot=live rd.live.image rd.live.dir=/live rd.live.squashimg=rootfs.squashfs tinexus.debug=1 console=ttyS0,115200n8 console=tty0
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
    info "Verifying & Building GRUB EFI image (BOOTX64.EFI)..."
    mkdir -p "$ISO_TREE/EFI/BOOT"

    # Explicit dual-tree module check for EFI
    local req_efi_mods=(gfxterm all_video gettext efi_gop font)
    for mod in "${req_efi_mods[@]}"; do
        [ -f "$GRUB_EFI_MODS/${mod}.mod" ] || fatal "Required GRUB EFI module missing: $GRUB_EFI_MODS/${mod}.mod"
    done

    local efi_modules=(
        part_gpt part_msdos fat exfat iso9660
        normal boot linux linux16 configfile
        search search_fs_uuid search_fs_file search_label
        gfxterm gfxterm_background all_video video_fb video efi_gop font gettext
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
    success "BOOTX64.EFI generated with full graphical modules embedded."
}

# ── GRUB BIOS Generation ──────────────────────────────────────────────────────
build_grub_bios() {
    info "Verifying & Building GRUB BIOS image (eltorito.img)..."
    mkdir -p "$ISO_TREE/boot/grub/i386-pc"

    # Explicit dual-tree module check for BIOS
    local req_bios_mods=(gfxterm all_video gettext vbe vga font)
    for mod in "${req_bios_mods[@]}"; do
        [ -f "$GRUB_BIOS_MODS/${mod}.mod" ] || fatal "Required GRUB BIOS module missing: $GRUB_BIOS_MODS/${mod}.mod"
    done

    local bios_modules=(
        biosdisk iso9660 normal linux search search_label search_fs_uuid
        configfile echo test reboot halt gfxterm all_video video video_fb vbe vga font gettext
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
    success "BIOS eltorito.img and hybrid MBR staged with full graphical modules."
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
