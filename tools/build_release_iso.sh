#!/usr/bin/env bash
# ==============================================================================
# Tinexus Platform — Real Hybrid Bootable ISO Builder (Production Grade)
#
# CRITICAL HARDWARE DRIVERS & FIRMWARE PRESERVATION LIST:
# DO NOT REMOVE the following modules or firmware without explicit architectural review:
# 1. Laptop Input / Touchpad (ASUS TUF / Intel LPSS / AMD I2C / Synaptics / ELAN):
#    - pinctrl-cannonlake, pinctrl-amd, intel-lpss, intel-lpss-pci
#    - i2c-designware-core, i2c-designware-pci, i2c-hid, i2c-hid-acpi
#    - hid-multitouch, psmouse, hid, hid-generic, usbhid, evdev
# 2. Wi-Fi / Networking:
#    - mt7921e, mt7921-common, iwlwifi, iwlmvm, rtw88_8821ce, rtw89_8852be
#    - All matching firmware under /lib/firmware (mediatek, intel, rtw88, rtw89)
# 3. Audio:
#    - snd-hda-intel, snd-hda-codec-realtek, snd-soc-sof, virtio_snd
#    - /etc/asound.conf targeting analog card (defaults.pcm.card, defaults.ctl.card)
# 4. Display / Graphics:
#    - i915, amdgpu, nouveau, bochs, virtio-gpu
#    - Intel DMC/GuC/HuC firmware under /lib/firmware/i915
#
# REQUIREMENTS before running:
#   1. build/kernel/vmlinuz      — Tinexus kernel image
#   2. build/kernel/initramfs.img — Tinexus initramfs (optional; built here if absent)
#   3. Host packages: xorriso squashfs-tools grub-efi-amd64-bin grub-pc-bin dosfstools mtools cpio gzip
#
# USAGE: bash build_release_iso.sh [--smoke-test]
# ==============================================================================
set -e -u -o pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="${PROJECT_DIR:-$(dirname "$SCRIPT_DIR")}"
BUILD_DIR="${BUILD_DIR:-$PROJECT_DIR/build}"
KERNEL_DIR="$BUILD_DIR/kernel"
OUTPUT_ISO="$BUILD_DIR/Tinexus-x86_64.iso"
ROOTFS_DIR="/var/tmp/tinexus_rootfs"
ISO_TREE="/var/tmp/tinexus_iso_tree"
WORK_DIR="/var/tmp/tinexus_iso_work"

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

    # Deterministically resolve ONE kernel version from installed kernels in build environment/chroot
    export KVER
    if [ -z "${KVER:-}" ]; then
        # Check installed modules under /lib/modules and match with /boot/vmlinuz-*
        local resolved_kver=""
        if [ -d "/lib/modules" ]; then
            for mod_dir in $(ls -d /lib/modules/* 2>/dev/null | sort -V -r); do
                local cand
                cand="$(basename "$mod_dir")"
                if [ -f "/boot/vmlinuz-$cand" ]; then
                    resolved_kver="$cand"
                    break
                fi
            done
        fi
        if [ -n "$resolved_kver" ]; then
            KVER="$resolved_kver"
            info "Deterministically resolved installed kernel version: $KVER"
            mkdir -p "$KERNEL_DIR"
            if [ ! -f "$KERNEL_DIR/vmlinuz" ] || [ "$(file -b "$KERNEL_DIR/vmlinuz" 2>/dev/null | sed -n 's/.*version \([^ ]*\).*/\1/p')" != "$KVER" ]; then
                if [ -r "/boot/vmlinuz-$KVER" ]; then
                    cp -L "/boot/vmlinuz-$KVER" "$KERNEL_DIR/vmlinuz"
                fi
            fi
        elif [ -f "$KERNEL_DIR/vmlinuz" ]; then
            KVER="$(file -b "$KERNEL_DIR/vmlinuz" | sed -n 's/.*version \([^ ]*\).*/\1/p')"
        else
            KVER="$(uname -r 2>/dev/null || echo '')"
            if [ -f "/boot/vmlinuz-$KVER" ] && [ -r "/boot/vmlinuz-$KVER" ]; then
                mkdir -p "$KERNEL_DIR"
                cp -L "/boot/vmlinuz-$KVER" "$KERNEL_DIR/vmlinuz"
            fi
        fi
    fi

    export VMLINUZ="$KERNEL_DIR/vmlinuz"
    [ -f "$VMLINUZ" ] || fatal "Tinexus kernel not found at: $VMLINUZ"
    if file -b "$VMLINUZ" | grep -qi "ASCII\|text"; then
        fatal "$VMLINUZ appears to be a text file, not a real kernel."
    fi

    # Verify that VMLINUZ's actual version matches KVER exactly
    local vmlinuz_ver
    vmlinuz_ver="$(file -b "$VMLINUZ" | sed -n 's/.*version \([^ ]*\).*/\1/p')"
    [ -n "$vmlinuz_ver" ] || fatal "Could not detect kernel version from $VMLINUZ"
    if [ -n "$KVER" ] && [ "$KVER" != "$vmlinuz_ver" ]; then
        if [ -f "/boot/vmlinuz-$KVER" ] && [ -r "/boot/vmlinuz-$KVER" ]; then
            info "Syncing vmlinuz to match KVER ($KVER)..."
            cp -L "/boot/vmlinuz-$KVER" "$VMLINUZ"
            vmlinuz_ver="$KVER"
        else
            warn "KVER ($KVER) differed from $VMLINUZ ($vmlinuz_ver); synchronizing KVER to $vmlinuz_ver"
            KVER="$vmlinuz_ver"
        fi
    fi

    [ -d "/lib/modules/$KVER" ] || fatal "Host module directory missing for kernel: /lib/modules/$KVER"
    info "Target kernel release synchronized: $KVER"

    # Verify vermagic of host modules against detected KVER
    local test_mod
    test_mod="$(find "/lib/modules/$KVER" -name "isofs.ko*" | head -n 1)"
    if [ -n "$test_mod" ]; then
        local vmag
        vmag="$(modinfo -F vermagic "$test_mod" 2>/dev/null | awk '{print $1}')"
        if [ -n "$vmag" ] && [ "$vmag" != "$KVER" ]; then
            fatal "Kernel vermagic mismatch: $test_mod has '$vmag' but expected '$KVER'"
        fi
        info "Verified module vermagic matches kernel: $vmag"
    fi

    # Check for SOF audio firmware package
    if [ ! -d "/lib/firmware/intel/sof-tplg" ] && [ ! -d "/usr/lib/firmware/intel/sof-tplg" ]; then
        warn "SOF audio firmware topology directory (/lib/firmware/intel/sof-tplg) not found on host.\nInstall: sudo apt install firmware-sof-signed"
    fi

    success "Toolchain & Environment OK."
}

# ── RootFS Generation via Debootstrap & APT ───────────────────────────────────
build_rootfs() {
    info "Building Tinexus root filesystem using pure Ubuntu resolute minbase..."

    # Ensure any previous virtual mounts are unmounted before wiping
    umount -lf "$ROOTFS_DIR/dev/pts" 2>/dev/null || true
    umount -lf "$ROOTFS_DIR/dev" 2>/dev/null || true
    umount -lf "$ROOTFS_DIR/sys" 2>/dev/null || true
    umount -lf "$ROOTFS_DIR/proc" 2>/dev/null || true

    rm -rf "$ROOTFS_DIR" "$ISO_TREE" "$WORK_DIR"
    mkdir -p "$ROOTFS_DIR"

    local suite="resolute"
    local mirror="http://archive.ubuntu.com/ubuntu/"
    local minbase_cache="$PROJECT_DIR/build/minbase-resolute.tar.gz"

    if [ -f "$minbase_cache" ]; then
        info "Unpacking cached Ubuntu resolute minbase from $minbase_cache..."
        tar -xzf "$minbase_cache" -C "$ROOTFS_DIR"
    else
        info "Running debootstrap --variant=minbase $suite..."
        debootstrap --variant=minbase "$suite" "$ROOTFS_DIR" "$mirror"
        info "Caching pristine minbase to $minbase_cache..."
        mkdir -p "$PROJECT_DIR/build"
        tar -czf "$minbase_cache" -C "$ROOTFS_DIR" .
    fi

    info "Configuring APT sources, DNS, and keyrings..."
    if [ -f /usr/share/keyrings/ubuntu-archive-keyring.gpg ]; then
        mkdir -p "$ROOTFS_DIR/usr/share/keyrings"
        cp -L /usr/share/keyrings/ubuntu-archive-keyring.gpg "$ROOTFS_DIR/usr/share/keyrings/"
    fi

    # Configure DNS in rootfs for apt inside chroot
    cp -L /etc/resolv.conf "$ROOTFS_DIR/etc/resolv.conf"

    # Configure APT sources for Ubuntu resolute
    mkdir -p "$ROOTFS_DIR/etc/apt/sources.list.d"
    cat > "$ROOTFS_DIR/etc/apt/sources.list.d/ubuntu.sources" << 'EOF_APT'
Types: deb
URIs: http://archive.ubuntu.com/ubuntu/
Suites: resolute resolute-updates resolute-security
Components: main restricted universe multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
EOF_APT

    # Blank out legacy sources.list
    > "$ROOTFS_DIR/etc/apt/sources.list"

    # ── Mozilla Firefox APT Repository (native deb, no snap) ────────────────
    # Per Mozilla's official Ubuntu installation guide: packages.mozilla.org
    # Add GPG keyring (fetched from host if available, otherwise wget inside chroot)
    info "Configuring Mozilla Firefox official APT repository..."
    mkdir -p "$ROOTFS_DIR/usr/share/keyrings"
    local MOZILLA_KEY="$ROOTFS_DIR/usr/share/keyrings/mozilla.gpg"
    if [ ! -f "$MOZILLA_KEY" ]; then
        # Fetch key on host and copy into rootfs
        wget -qO "$MOZILLA_KEY" "https://packages.mozilla.org/apt/repo-signing-key.gpg" || \
        curl -fsSL "https://packages.mozilla.org/apt/repo-signing-key.gpg" -o "$MOZILLA_KEY" || \
        warn "Could not fetch Mozilla GPG key — Firefox may not install from packages.mozilla.org"
    fi
    if [ -f "$MOZILLA_KEY" ]; then
        cat > "$ROOTFS_DIR/etc/apt/sources.list.d/mozilla.sources" << 'EOF_MOZ'
Types: deb
URIs: https://packages.mozilla.org/apt
Suites: mozilla
Components: main
Signed-By: /usr/share/keyrings/mozilla.gpg
EOF_MOZ
        # Pin Firefox from Mozilla repo above Ubuntu universe to avoid snap redirect
        mkdir -p "$ROOTFS_DIR/etc/apt/preferences.d"
        cat > "$ROOTFS_DIR/etc/apt/preferences.d/mozilla-firefox" << 'EOF_PIN'
Package: *
Pin: origin packages.mozilla.org
Pin-Priority: 1001
EOF_PIN
        info "Mozilla Firefox APT repository configured."
    fi

    # Prevent daemons from auto-starting during apt install
    cat > "$ROOTFS_DIR/usr/sbin/policy-rc.d" << 'EOF_POLICY'
#!/bin/sh
exit 101
EOF_POLICY
    chmod +x "$ROOTFS_DIR/usr/sbin/policy-rc.d"

    # Bind mount virtual filesystems for chroot
    info "Mounting proc, sys, dev into rootfs chroot..."
    mount --bind /proc "$ROOTFS_DIR/proc"
    mount --bind /sys "$ROOTFS_DIR/sys"
    mount --bind /dev "$ROOTFS_DIR/dev"
    mount --bind /dev/pts "$ROOTFS_DIR/dev/pts"

    # APT package list to install inside chroot
    info "Running apt-get update and installing runtime packages inside chroot..."
    chroot "$ROOTFS_DIR" env -i \
        DEBIAN_FRONTEND=noninteractive \
        PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
        apt-get update

    chroot "$ROOTFS_DIR" env -i \
        DEBIAN_FRONTEND=noninteractive \
        PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
        apt-get install -y --no-install-recommends \
            udev kmod dbus dbus-daemon \
            iproute2 net-tools iputils-ping \
            wpasupplicant iw rfkill curl ca-certificates \
            fuse3 libfuse3-4 busybox sudo locales \
            alsa-utils libasound2t64 brightnessctl \
            mesa-va-drivers mesa-vulkan-drivers libgl1-mesa-dri \
            libegl-mesa0 libgbm1 \
            libwayland-client0 libwayland-server0 libwayland-cursor0 libwayland-egl1 \
            libxkbcommon0 libpixman-1-0 libinput10 libseat1 \
            libwlroots-0.19 libliftoff0 liblayershellqtinterface6 layer-shell-qt \
            libxcb-errors0 libxcb-ewmh2 libxcb-icccm4 \
            qt6-wayland libqt6core6t64 libqt6gui6 libqt6qml6 libqt6quick6 \
            libqt6quicktemplates2-6 libqt6quickcontrols2-6 \
            qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts \
            qml6-module-qtquick-templates qml6-module-qtquick-window qml6-module-qtquick-dialogs \
            qml6-module-qtquick-effects \
            foot fonts-dejavu-core fonts-liberation fontconfig \
            libpam-runtime libpam-modules \
            isc-dhcp-client procps libcap2-bin \
            parted e2fsprogs dosfstools squashfs-tools grub-efi-amd64-bin \
            firefox

    # ── Firefox: Stage Wayland profile & macOS traffic-light userChrome ────
    # Firefox profile lands in /etc/skel/.mozilla so every user inherits it
    info "Staging Firefox Wayland profile and Tinexus-themed userChrome.css..."
    local FF_PROFILE="$ROOTFS_DIR/etc/skel/.mozilla/firefox/tinexus.default"
    mkdir -p "$FF_PROFILE/chrome"

    # user.js — enable userChrome.css and configure Wayland environment
    cat > "$FF_PROFILE/user.js" << 'EOF_USERJS'
// Tinexus Firefox Profile — auto-applied on first launch
// Enable custom UI CSS
user_pref("toolkit.legacyUserProfileCustomizations.stylesheets", true);
// Prefer Wayland (EGL) rendering
user_pref("gfx.webrender.enabled", true);
user_pref("media.ffmpeg.vaapi.enabled", true);
// Compact mode
user_pref("browser.uidensity", 1);
user_pref("browser.in-content.dark-mode", true);
user_pref("browser.tabs.inTitlebar", 1);
user_pref("browser.tabs.drawInTitlebar", true);
// Disable telemetry
user_pref("datareporting.healthreport.uploadEnabled", false);
user_pref("datareporting.policy.dataSubmissionEnabled", false);
EOF_USERJS

    # userChrome.css — hide internal CSD buttons so Tinexus SSD is the single outer frame
    cat > "$FF_PROFILE/chrome/userChrome.css" << 'EOF_CHROME'
/* Tinexus Firefox userChrome.css — single frame configuration */
@namespace url("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul");

/* When running under Tinexus Compositor SSD (Server-Side Decorations),
   hide Firefox's internal CSD window control buttons so exactly ONE outer frame remains. */
.titlebar-buttonbox-container,
.titlebar-buttonbox,
.titlebar-button {
    display: none !important;
}

#TabsToolbar {
    margin-left: 0 !important;
    padding-left: 0 !important;
}
#TabsToolbar .tabbrowser-tab { min-height: 28px !important; }
#navigator-toolbox { padding-top: 0 !important; }
EOF_CHROME

    # profiles.ini — tell Firefox to use this profile by default
    cat > "$ROOTFS_DIR/etc/skel/.mozilla/firefox/profiles.ini" << 'EOF_PROFILES'
[Profile0]
Name=tinexus
IsRelative=1
Path=tinexus.default
Default=1

[General]
StartWithLastProfile=1
Version=2
EOF_PROFILES

    # installs.ini — point default install to this profile
    cat > "$ROOTFS_DIR/etc/skel/.mozilla/firefox/installs.ini" << 'EOF_INSTALLS'
[Install]
Default=tinexus.default
Locked=1
EOF_INSTALLS

    # Firefox .desktop entry with Wayland native flags
    mkdir -p "$ROOTFS_DIR/usr/share/applications"
    cat > "$ROOTFS_DIR/usr/share/applications/firefox.desktop" << 'EOF_FFDESKTOP'
[Desktop Entry]
Name=Firefox
GenericName=Web Browser
Comment=Browse the World Wide Web
Exec=env MOZ_ENABLE_WAYLAND=1 firefox %u
Icon=firefox
Terminal=false
Type=Application
MimeType=text/html;text/xml;application/xhtml+xml;x-scheme-handler/http;x-scheme-handler/https;
Categories=Network;WebBrowser;Internet;
StartupNotify=true
EOF_FFDESKTOP
    chmod 0644 "$ROOTFS_DIR/usr/share/applications/firefox.desktop"
    success "Firefox Wayland profile and Tinexus userChrome.css staged into /etc/skel."

    # Pre-cache VLC and common test packages in apt cache
    info "Pre-caching all VLC deb packages inside rootfs apt archive cache..."
    chroot "$ROOTFS_DIR" env -i \
        DEBIAN_FRONTEND=noninteractive \
        PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
        apt-get install -d -y --no-install-recommends vlc || true

    # Build and package Tinexus .deb packages
    info "Generating Tinexus modular Debian packages..."
    bash "$PROJECT_DIR/tools/package_tinexus_debs.sh" "$PROJECT_DIR/build/debs"

    # Stage debs into chroot /tmp/debs and install via dpkg
    info "Installing Tinexus packages inside chroot..."
    mkdir -p "$ROOTFS_DIR/tmp/debs"
    cp "$PROJECT_DIR/build/debs"/*.deb "$ROOTFS_DIR/tmp/debs/"
    chroot "$ROOTFS_DIR" env -i \
        DEBIAN_FRONTEND=noninteractive \
        PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
        dpkg -i --force-overwrite /tmp/debs/tinexus-core_1.0.0_amd64.deb \
                /tmp/debs/tinexus-compositor_1.0.0_amd64.deb \
                /tmp/debs/tinexus-desktop_1.0.0_amd64.deb \
                /tmp/debs/tinexus-apps_1.0.0_amd64.deb
    rm -rf "$ROOTFS_DIR/tmp/debs"

    # Run ldconfig to update shared library cache inside rootfs
    info "Running ldconfig inside rootfs chroot..."
    chroot "$ROOTFS_DIR" env -i \
        PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
        ldconfig

    # ── Grant file capabilities to network binaries (setcap model) ─────────────
    # This allows tinexus user (UID 1000) to invoke dhclient/wpa_supplicant/ip
    # directly without sudo. cap_net_admin grants SIOCSIFADDR/ip-route access;
    # cap_net_raw grants AF_PACKET socket for DHCP DISCOVER; cap_net_bind_service
    # allows binding to port 68. This is the standard Linux approach used by
    # NetworkManager itself — no NOPASSWD:ALL sudo policy needed at runtime.
    info "Applying file capabilities to network binaries via setcap..."
    # dhclient — isc-dhcp-client
    for dhclient_bin in \
        "$ROOTFS_DIR/usr/sbin/dhclient" \
        "$ROOTFS_DIR/sbin/dhclient"; do
        if [ -f "$dhclient_bin" ]; then
            setcap cap_net_admin,cap_net_raw,cap_net_bind_service+ep "$dhclient_bin"
            info "setcap applied to $dhclient_bin"
        fi
    done
    # wpa_supplicant
    for wpa_bin in \
        "$ROOTFS_DIR/usr/sbin/wpa_supplicant" \
        "$ROOTFS_DIR/sbin/wpa_supplicant"; do
        if [ -f "$wpa_bin" ]; then
            setcap cap_net_admin,cap_net_raw+ep "$wpa_bin"
            info "setcap applied to $wpa_bin"
        fi
    done
    # ip (iproute2)
    for ip_bin in \
        "$ROOTFS_DIR/usr/sbin/ip" \
        "$ROOTFS_DIR/usr/bin/ip" \
        "$ROOTFS_DIR/sbin/ip" \
        "$ROOTFS_DIR/bin/ip"; do
        if [ -f "$ip_bin" ]; then
            setcap cap_net_admin+ep "$ip_bin"
            info "setcap applied to $ip_bin"
        fi
    done
    # /var/lib/dhcp must be writable by tinexus user for dhclient lease file fallback
    mkdir -p "$ROOTFS_DIR/var/lib/dhcp"
    chmod 1777 "$ROOTFS_DIR/var/lib/dhcp"
    info "setcap network capability configuration complete."
    success "Network binaries are capability-hardened (no sudo needed for tinexus user)."

    # Strict check: Confirm libtinexus_common.so.0 is present in rootfs
    info "Verifying libtinexus_common.so.0 presence in rootfs..."
    if [ ! -e "$ROOTFS_DIR/usr/lib/x86_64-linux-gnu/libtinexus_common.so.0" ] && [ ! -e "$ROOTFS_DIR/usr/lib/libtinexus_common.so.0" ]; then
        fatal "CRITICAL ERROR: libtinexus_common.so.0 is missing from rootfs /usr/lib!"
    fi
    success "Verified libtinexus_common.so.0 is present in rootfs."

    # Strict check: Confirm ldconfig cache inside rootfs resolves libtinexus_common.so.0
    info "Verifying ld.so.cache registration in rootfs chroot..."
    chroot "$ROOTFS_DIR" env -i \
        PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
        ldconfig -p | grep -q "libtinexus_common.so.0" || fatal "CRITICAL ERROR: libtinexus_common.so.0 missing from ldconfig cache!"
    success "Verified libtinexus_common.so.0 is registered in ld.so.cache."

    # Strict check: Verify Qt6 Wayland layer-shell plugin is installed
    info "Verifying Qt6 Wayland layer-shell integration plugin in rootfs..."
    if [ ! -f "$ROOTFS_DIR/usr/lib/x86_64-linux-gnu/qt6/plugins/wayland-shell-integration/liblayer-shell.so" ]; then
        fatal "CRITICAL ERROR: liblayer-shell.so is missing from rootfs Qt6 wayland-shell-integration plugins!"
    fi
    success "Verified liblayer-shell.so plugin is present in rootfs."

    # Strict check: Test ldd on all primary Tinexus binaries inside rootfs chroot
    info "Verifying shared library resolution for all Tinexus binaries inside rootfs chroot..."
    for chk_bin in tinexus-serviced tinexus-ipcd tinexus-session tinexus-comp tinexus-launcher tinexus-settings tinexus-shell tinexus-dock tinexus-wallpaper tinexus-lock tinexus-notifications; do
        if [ -x "$ROOTFS_DIR/usr/bin/$chk_bin" ]; then
            local unresolved
            unresolved=$(chroot "$ROOTFS_DIR" env -i PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin ldd "/usr/bin/$chk_bin" 2>&1 | grep "not found" || true)
            if [ -n "$unresolved" ]; then
                fatal "CRITICAL ERROR: $chk_bin has unresolved shared libraries:\n$unresolved"
            fi
            info "Verified $chk_bin shared libraries resolve cleanly."
        fi
    done
    success "Verified all primary Tinexus binaries resolve shared libraries cleanly inside rootfs chroot!"

    # ── Systematic Automated Rootfs Verification Pass ────────────────────────
    info "Running automated assertion: Verifying EVERY file promised by ANY .deb package exists in rootfs..."
    local missing_files=0
    for deb in "$PROJECT_DIR/build/debs"/*.deb; do
        [ -f "$deb" ] || continue
        local pkg_name
        pkg_name="$(dpkg-deb -f "$deb" Package 2>/dev/null || basename "$deb")"
        info "Auditing packaged payload of '$pkg_name' ($(basename "$deb"))..."
        
        while IFS= read -r line; do
            [[ "$line" =~ ^d ]] && continue
            local raw_target
            raw_target="$(echo "$line" | awk '{print $NF}')"
            if [[ "$line" =~ \ -\>\  ]]; then
                raw_target="$(echo "$line" | sed -n 's/.* \(\.\/[^ ]*\) -> .*/\1/p')"
            fi
            local clean_target="${raw_target#./}"
            [ -n "$clean_target" ] || continue
            
            if [ ! -e "$ROOTFS_DIR/$clean_target" ] && [ ! -L "$ROOTFS_DIR/$clean_target" ]; then
                echo -e "\e[1;31m[MISSING FILE ERROR]\e[0m Package '$pkg_name' promises '/$clean_target', but it is MISSING from rootfs!" >&2
                missing_files=$((missing_files + 1))
            fi
        done < <(dpkg-deb -c "$deb")
    done

    if [ "$missing_files" -gt 0 ]; then
        fatal "Automated rootfs assertion FAILED: $missing_files packaged files are missing from rootfs!"
    fi
    success "Automated rootfs assertion PASSED: 100% of files across all .deb packages are confirmed present in rootfs."

    # Ensure appimage runner script exists
    if [ -f "$PROJECT_DIR/tools/tinexus-appimage-runner.sh" ]; then
        cp -L "$PROJECT_DIR/tools/tinexus-appimage-runner.sh" "$ROOTFS_DIR/usr/bin/tx-appimage"
        chmod 0755 "$ROOTFS_DIR/usr/bin/tx-appimage"
    fi

    # Set up user 'tinexus' (UID 1000)
    info "Configuring default live user and permissions..."
    chroot "$ROOTFS_DIR" env -i PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
        /bin/bash -c "id -u tinexus >/dev/null 2>&1 || useradd -m -s /bin/bash -u 1000 tinexus; usermod -aG sudo,audio,video,input,render tinexus 2>/dev/null || true; echo 'tinexus:tinexus' | chpasswd; echo 'root:root' | chpasswd"

    # Passwordless sudo for tinexus live session
    mkdir -p "$ROOTFS_DIR/etc/sudoers.d"
    echo "tinexus ALL=(ALL) NOPASSWD:ALL" > "$ROOTFS_DIR/etc/sudoers.d/tinexus-live"
    chmod 0440 "$ROOTFS_DIR/etc/sudoers.d/tinexus-live"

    # System identification & Hostname
    cat > "$ROOTFS_DIR/etc/os-release" << 'EOF_OS'
NAME="Tinexus OS"
VERSION="1.0"
ID=tinexus
ID_LIKE=ubuntu
PRETTY_NAME="Tinexus OS 1.0 (Live)"
HOME_URL="https://github.com/itzabhishekgour/TinexusShell"
EOF_OS

    echo "tinexus-desktop" > "$ROOTFS_DIR/etc/hostname"
    cat > "$ROOTFS_DIR/etc/hosts" << 'EOF_HOSTS'
127.0.0.1 localhost
127.0.1.1 tinexus-desktop
EOF_HOSTS

    # Locale generation
    info "Generating locales..."
    chroot "$ROOTFS_DIR" env -i PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
        /bin/bash -c "locale-gen en_US.UTF-8 && update-locale LANG=en_US.UTF-8" || true

    # Shell profile
    cat > "$ROOTFS_DIR/etc/profile" << 'EOF_PROFILE'
export PS1='\e[01;32m\u@\h\e[00m:\e[01;34m\w\e[00m\$ '
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
export QT_QPA_PLATFORM=wayland
export QT_PLUGIN_PATH=/usr/lib/x86_64-linux-gnu/qt6/plugins
export QML2_IMPORT_PATH=/usr/lib/x86_64-linux-gnu/qt6/qml
export QML_IMPORT_PATH=/usr/lib/x86_64-linux-gnu/qt6/qml
export TINEXUS_SETTINGS_QML=/usr/share/tinexus-settings/qml/MainWindow.qml
EOF_PROFILE

    # Ensure Supervisor PID 1 symlinks
    mkdir -p "$ROOTFS_DIR/sbin" "$ROOTFS_DIR/bin"
    ln -sf /usr/bin/tinexus-serviced "$ROOTFS_DIR/sbin/init"
    ln -sf /usr/bin/tinexus-serviced "$ROOTFS_DIR/init"

    # Audio priority config (auto-detect dsp_driver=0 for universal hardware compatibility)
    mkdir -p "$ROOTFS_DIR/etc/modprobe.d"
    cat << 'EOF_SOF' > "$ROOTFS_DIR/etc/modprobe.d/sof-priority.conf"
options snd-intel-dspcfg dsp_driver=0
softdep snd_hda_intel pre: snd_sof_pci_intel_cnl snd_sof_pci_intel_icl snd_sof_pci_intel_tgl snd_sof_pci_intel_mtl snd_sof_intel_hda_generic
softdep snd_sof_intel_hda_generic pre: snd_soc_hdac_hda snd_soc_skl_hda_dsp
EOF_SOF

    # Copy kernel vmlinuz and initrd into rootfs /boot so installed systems have boot components
    mkdir -p "$ROOTFS_DIR/boot"
    cp -L "$VMLINUZ" "$ROOTFS_DIR/boot/vmlinuz-$KVER"
    ln -sf "vmlinuz-$KVER" "$ROOTFS_DIR/boot/vmlinuz"
    if [ -f "/boot/initrd.img-$KVER" ]; then
        cp -L "/boot/initrd.img-$KVER" "$ROOTFS_DIR/boot/initrd.img-$KVER"
        ln -sf "initrd.img-$KVER" "$ROOTFS_DIR/boot/initrd.img"
        info "Staged host initrd.img-$KVER into rootfs /boot for target bare-metal installations."
    elif [ -f "/boot/initrd.img" ]; then
        cp -L "/boot/initrd.img" "$ROOTFS_DIR/boot/initrd.img"
    fi

    # Copy host kernel modules and firmware
    info "Staging kernel modules ($KVER) and firmware into rootfs..."
    mkdir -p "$ROOTFS_DIR/lib/modules"
    if [ -d "/lib/modules/$KVER" ]; then
        cp -a "/lib/modules/$KVER" "$ROOTFS_DIR/lib/modules/"
        chroot "$ROOTFS_DIR" env -i PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
            depmod -a "$KVER" 2>/dev/null || true
    fi

    if [ -d "/lib/firmware" ]; then
        mkdir -p "$ROOTFS_DIR/lib/firmware"
        for fw_sub in i915 intel mediatek rtw88 rtw89; do
            if [ -d "/lib/firmware/$fw_sub" ]; then
                cp -a "/lib/firmware/$fw_sub" "$ROOTFS_DIR/lib/firmware/" 2>/dev/null || true
            fi
        done
    fi

    # Clean up policy-rc.d and unmount virtual filesystems
    rm -f "$ROOTFS_DIR/usr/sbin/policy-rc.d"
    info "Preserving pre-cached archives in rootfs ($(ls -1 "$ROOTFS_DIR/var/cache/apt/archives"/*.deb 2>/dev/null | wc -l) packages)..."

    info "Unmounting chroot virtual filesystems..."
    umount -lf "$ROOTFS_DIR/dev/pts" 2>/dev/null || true
    umount -lf "$ROOTFS_DIR/dev" 2>/dev/null || true
    umount -lf "$ROOTFS_DIR/sys" 2>/dev/null || true
    umount -lf "$ROOTFS_DIR/proc" 2>/dev/null || true

    # Run ldconfig to cache all shared libraries
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
    if [ -f "$PROJECT_DIR/assets/logo/tinexus-logo.png" ]; then
        cp -L "$PROJECT_DIR/assets/logo/tinexus-logo.png" "$init_staging/tinexus-logo.png"
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
        mkdir -p "$init_staging/sbin" "$init_staging/bin"
        ln -sf ../bin/kmod "$init_staging/sbin/modprobe"
        ln -sf kmod "$init_staging/bin/modprobe"
        ln -sf ../bin/kmod "$init_staging/sbin/depmod"
        ln -sf kmod "$init_staging/bin/depmod"
        ldd "/usr/bin/kmod" 2>/dev/null | sed -n 's/.*=> \(.*\) (0x.*/\1/p' | while read -r lib; do
            [ -f "$lib" ] && { mkdir -p "$init_staging$(dirname "$lib")"; cp -L "$lib" "$init_staging$lib" 2>/dev/null || true; }
        done
        ldd "/usr/bin/kmod" 2>/dev/null | sed -n 's/^[[:space:]]*\(\/.*\) (0x.*/\1/p' | while read -r ld_loader; do
            [ -f "$ld_loader" ] && { mkdir -p "$init_staging$(dirname "$ld_loader")"; cp -L "$ld_loader" "$init_staging$ld_loader" 2>/dev/null || true; }
        done
    fi

    # Stage modprobe priority configuration in initramfs (auto-detect dsp_driver=0)
    info "Staging audio driver priority configuration into initramfs (/etc/modprobe.d/sof-priority.conf)..."
    mkdir -p "$init_staging/etc/modprobe.d"
    cat << 'EOF' > "$init_staging/etc/modprobe.d/sof-priority.conf"
# Universal DSP driver configuration for Intel hardware platforms (0=auto-detect)
options snd-intel-dspcfg dsp_driver=0

# Ensure SOF drivers are loaded and preferred before snd_hda_intel on Intel DSP platforms
softdep snd_hda_intel pre: snd_sof_pci_intel_cnl snd_sof_pci_intel_icl snd_sof_pci_intel_tgl snd_sof_pci_intel_mtl snd_sof_intel_hda_generic

# Ensure generic ASoC HDA bridge and DSP machine driver are available with SOF
softdep snd_sof_intel_hda_generic pre: snd_soc_hdac_hda snd_soc_skl_hda_dsp
EOF

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

    mkdir -p "$init_staging/lib/modules/$KVER" "$init_staging/lib/modules"
    if [ -d "/lib/modules/$KVER" ]; then
        find "/lib/modules/$KVER" -type f \( \
            -name "isofs.ko*" -o -name "udf.ko*" \
            -o -name "nls_utf8.ko*" -o -name "nls_iso8859-1.ko*" -o -name "nls_cp437.ko*" -o -name "nls_ascii.ko*" \
            -o -name "ahci.ko*" -o -name "libahci.ko*" \
            -o -name "nvme.ko*" -o -name "nvme-core.ko*" -o -name "nvme-auth.ko*" -o -name "nvme-keyring.ko*" -o -name "hkdf.ko*" \
            -o -name "uas.ko*" -o -name "usb-storage.ko*" \
            -o -name "xhci-pci.ko*" -o -name "xhci-hcd.ko*" -o -name "ehci-pci.ko*" -o -name "ehci-hcd.ko*" \
            -o -name "virtio.ko*" -o -name "virtio_ring.ko*" -o -name "virtio_pci.ko*" -o -name "virtio_pci_modern_dev.ko*" \
            -o -name "virtio_blk.ko*" -o -name "virtio_scsi.ko*" \
            -o -name "overlay.ko*" \
            -o -name "i915.ko*" -o -name "drm_display_helper.ko*" -o -name "ttm.ko*" -o -name "video.ko*" \
            -o -name "drm_buddy.ko*" -o -name "cec.ko*" -o -name "rc-core.ko*" -o -name "i2c-algo-bit.ko*" -o -name "wmi.ko*" -o -name "intel-gtt.ko*" \
            -o -name "virtio-gpu.ko*" -o -name "virtio_dma_buf.ko*" -o -name "bochs.ko*" \
            -o -name "virtio_input.ko*" \
            -o -name "evdev.ko*" \
            -o -name "hid.ko*" -o -name "hid-generic.ko*" -o -name "usbhid.ko*" \
            -o -name "pinctrl-*.ko*" \
            -o -name "intel-lpss.ko*" -o -name "intel-lpss-pci.ko*" \
            -o -name "i2c-core.ko*" -o -name "i2c-algo-bit.ko*" \
            -o -name "i2c-designware-core.ko*" -o -name "i2c-designware-pci.ko*" -o -name "i2c-designware-platform.ko*" \
            -o -name "i2c-hid.ko*" -o -name "i2c-hid-acpi.ko*" \
            -o -name "hid-multitouch.ko*" -o -name "psmouse.ko*" \
        \) | while read -r mod; do
            cp -L "$mod" "$init_staging/lib/modules/$KVER/"
            cp -L "$mod" "$init_staging/lib/modules/"
        done
        for compressed in "$init_staging/lib/modules/$KVER"/*.zst "$init_staging/lib/modules"/*.zst; do
            [ -f "$compressed" ] && zstd -d --rm "$compressed" 2>/dev/null || true
        done
        cp /lib/modules/"$KVER"/modules.order "$init_staging/lib/modules/$KVER/" 2>/dev/null || true
        cp /lib/modules/"$KVER"/modules.builtin* "$init_staging/lib/modules/$KVER/" 2>/dev/null || true
        if [ -f "/usr/sbin/depmod" ]; then
            /usr/sbin/depmod -b "$init_staging" "$KVER" 2>/dev/null || true
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
CMDLINE=""
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

KVER="$(uname -r 2>/dev/null || echo '')"

load_mod() {
    local m="$1"
    # Try modprobe first
    modprobe -d / "$m" 2>/dev/null
    if [ $? -eq 0 ]; then
        return 0
    fi
    # Direct insmod fallback
    for p in "/lib/modules/$KVER" /lib/modules; do
        if [ -f "$p/$m.ko" ]; then
            insmod "$p/$m.ko" 2>/dev/null && return 0
        fi
    done
    return 1
}

# 1. Storage & bus controllers (with sub-dependencies and NLS charsets)
# Universal: USB xHCI/EHCI, SCSI, SATA AHCI, NVMe, VirtIO, NLS charsets, ISOFS, UDF, OverlayFS
for m in virtio virtio_ring virtio_pci virtio_pci_modern_dev virtio_blk virtio_scsi \
         xhci-hcd xhci-pci ehci-hcd ehci-pci libahci ahci \
         nls_cp437 nls_iso8859-1 nls_utf8 nls_ascii isofs udf \
         hkdf nvme-keyring nvme-auth nvme-core nvme usb-storage uas overlay; do
    load_mod "$m" || true
done

# 2. Universal input & platform device coldplug via sysfs MODALIAS autoloading
log_step "Autoloading input and platform bus drivers via sysfs MODALIAS..."
find /sys/devices -name modalias 2>/dev/null | while read -r mfile; do
    [ -f "$mfile" ] || continue
    alias="$(cat "$mfile" 2>/dev/null)"
    [ -n "$alias" ] && modprobe -b -q "$alias" 2>/dev/null || true
done

# Fallback direct load for standard input & HID drivers
for m in psmouse hid hid-generic usbhid evdev i2c-hid i2c-hid-acpi hid-multitouch virtio_input; do
    load_mod "$m" || true
done

# 3. Graphics display drivers (Early display / splash fallback)
for m in rc-core cec drm_display_helper wmi video ttm drm_buddy \
         i2c-algo-bit i915 virtio_dma_buf virtio-gpu bochs; do
    load_mod "$m" || true
done

# Allow USB bus and storage controllers settling time
sleep 2
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

# Parse kernel command line parameters for hints
TARGET_LABEL="TINEXUS_LIVE"
TARGET_UUID=""
SQUASH_PATH="live/rootfs.squashfs"

if [ -n "$CMDLINE" ]; then
    for param in $CMDLINE; do
        case "$param" in
            root=live:CDLABEL=*)
                TARGET_LABEL="${param#root=live:CDLABEL=}"
                ;;
            root=live:LABEL=*)
                TARGET_LABEL="${param#root=live:LABEL=}"
                ;;
            root=live:UUID=*)
                TARGET_UUID="${param#root=live:UUID=}"
                ;;
            root=LABEL=*)
                TARGET_LABEL="${param#root=LABEL=}"
                ;;
            root=UUID=*)
                TARGET_UUID="${param#root=UUID=}"
                ;;
            rd.live.dir=*)
                LIVE_DIR="${param#rd.live.dir=}"
                LIVE_DIR="${LIVE_DIR#/}"
                ;;
            rd.live.squashimg=*)
                SQUASH_IMG="${param#rd.live.squashimg=}"
                ;;
        esac
    done
    if [ -n "$LIVE_DIR" ] && [ -n "$SQUASH_IMG" ]; then
        SQUASH_PATH="$LIVE_DIR/$SQUASH_IMG"
    fi
fi

mkdir -p /dev/disk/by-label /dev/disk/by-uuid /mnt

ROOT_DEV=""

# Function to mount a candidate block device smartly based on detected filesystem type
try_mount_candidate() {
    local cand="$1"
    local fstype="$2"
    [ -b "$cand" ] || return 1

    # Determine filesystem mount options based on detected fstype
    local opts_list=""
    case "$fstype" in
        iso9660) opts_list="-t iso9660 -o ro;-o ro" ;;
        vfat|fat|msdos) opts_list="-t vfat -o ro;-o ro" ;;
        udf) opts_list="-t udf -o ro;-o ro" ;;
        ext*|btrfs|xfs) opts_list="-o ro" ;;
        *) opts_list="-t iso9660 -o ro;-o ro;-t vfat -o ro" ;;
    esac

    local old_ifs="$IFS"
    IFS=";"
    for mopts in $opts_list; do
        IFS="$old_ifs"
        if /bin/mount $mopts "$cand" /mnt 2>/dev/null; then
            # Verify authoritative squashfs path or fallback path
            if [ -f "/mnt/$SQUASH_PATH" ] && [ -s "/mnt/$SQUASH_PATH" ]; then
                log_step "SUCCESS: Found $SQUASH_PATH on $cand (mount $mopts)"
                ROOT_DEV="$cand"
                IFS="$old_ifs"
                return 0
            elif [ -f "/mnt/rootfs.squashfs" ] && [ -s "/mnt/rootfs.squashfs" ]; then
                log_step "SUCCESS: Found rootfs.squashfs on $cand (mount $mopts)"
                SQUASH_PATH="rootfs.squashfs"
                ROOT_DEV="$cand"
                IFS="$old_ifs"
                return 0
            fi
            /bin/umount /mnt 2>/dev/null || true
        fi
        IFS=";"
    done
    IFS="$old_ifs"
    return 1
}

# Exhaustive retry loop (up to 20 attempts, 1s interval)
for attempt in $(seq 1 20); do
    echo $((20 + attempt * 3)) > /tmp/splash_progress 2>/dev/null
    /bin/mdev -s 2>/dev/null

    # Collect all candidate block devices from /sys/block/
    PRIORITY_CANDIDATES=""
    OTHER_CANDIDATES=""

    for b in /sys/block/*; do
        [ -e "$b" ] || continue
        devname=$(basename "$b")
        case "$devname" in
            loop*|ram*|zram*) continue ;;
        esac

        # Check whole device first, then any numbered/partition sub-devices
        for cand in "/dev/$devname" "/dev/${devname}"p* "/dev/${devname}"[0-9]*; do
            [ -b "$cand" ] || continue

            # Probe block device details with blkid
            CAND_TYPE=""
            CAND_LABEL=""
            CAND_UUID=""
            if [ -x /bin/blkid ]; then
                BLK_OUT=$(/bin/blkid "$cand" 2>/dev/null || true)
                if [ -n "$BLK_OUT" ]; then
                    CAND_TYPE=$(echo "$BLK_OUT" | sed -n 's/.*TYPE="\([^"]*\)".*/\1/p')
                    CAND_LABEL=$(echo "$BLK_OUT" | sed -n 's/.*LABEL="\([^"]*\)".*/\1/p')
                    CAND_UUID=$(echo "$BLK_OUT" | sed -n 's/.*UUID="\([^"]*\)".*/\1/p')

                    # Create dynamic by-label and by-uuid symlinks
                    [ -n "$CAND_LABEL" ] && ln -sf "$cand" "/dev/disk/by-label/$CAND_LABEL" 2>/dev/null || true
                    [ -n "$CAND_UUID" ] && ln -sf "$cand" "/dev/disk/by-uuid/$CAND_UUID" 2>/dev/null || true
                fi
            fi

            # Check if this candidate matches target label or target UUID
            is_priority=0
            if [ -n "$TARGET_LABEL" ] && [ "$CAND_LABEL" = "$TARGET_LABEL" ]; then
                is_priority=1
            elif [ -n "$TARGET_UUID" ] && [ "$CAND_UUID" = "$TARGET_UUID" ]; then
                is_priority=1
            fi

            if [ "$is_priority" = "1" ]; then
                PRIORITY_CANDIDATES="$PRIORITY_CANDIDATES $cand|$CAND_TYPE"
            else
                OTHER_CANDIDATES="$OTHER_CANDIDATES $cand|$CAND_TYPE"
            fi
        done
    done

    # Test priority candidates first (label / UUID match)
    for entry in $PRIORITY_CANDIDATES; do
        cand="${entry%%|*}"
        fstype="${entry#*|}"
        if try_mount_candidate "$cand" "$fstype"; then
            break 2
        fi
    done

    # Then test all other block device candidates
    for entry in $OTHER_CANDIDATES; do
        cand="${entry%%|*}"
        fstype="${entry#*|}"
        if try_mount_candidate "$cand" "$fstype"; then
            break 2
        fi
    done

    sleep 1
done

if [ -z "$ROOT_DEV" ] || [ ! -f "/mnt/$SQUASH_PATH" ]; then
    log_step "================================================================"
    log_step "FATAL: $SQUASH_PATH not found on any storage device after 20s!"
    log_step "================================================================"
    log_step "--- DIAGNOSTIC SUMMARY FOR BOOT FAILURE TRIAGE ---"
    log_step "Kernel release (uname -r): $(uname -r 2>/dev/null)"
    log_step "Kernel cmdline: $CMDLINE"
    log_step "1. Detected block devices in /proc/partitions:"
    cat /proc/partitions > /dev/console 2>&1 || true
    log_step "2. Detected USB devices in /sys/bus/usb/devices/:"
    ls -la /sys/bus/usb/devices/ > /dev/console 2>&1 || true
    log_step "3. blkid output across all block nodes:"
    /bin/blkid /dev/sd* /dev/sr* /dev/nvme* /dev/vd* 2>/dev/null > /dev/console 2>&1 || true
    log_step "4. Loaded kernel modules:"
    cat /proc/modules > /dev/console 2>&1 || true
    log_step "================================================================"
    log_step "Dropping to emergency recovery shell on /dev/console..."
    exec /bin/sh </dev/console >/dev/console 2>&1
fi

log_step "STEP 5: Storage media located on $ROOT_DEV (mount path: /mnt/$SQUASH_PATH)."
mkdir -p /newroot

log_step "STEP 6: Setting up writable OverlayFS (RAM tmpfs + SquashFS lowerdir)..."
mkdir -p /rofs /cow /newroot
/bin/mount -t squashfs -o ro "/mnt/$SQUASH_PATH" /rofs
MNT_STATUS=$?
if [ $MNT_STATUS -ne 0 ]; then
    log_step "FATAL: mount -t squashfs /mnt/$SQUASH_PATH failed with exit code $MNT_STATUS!"
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
    /bin/mount -t squashfs -o ro "/mnt/$SQUASH_PATH" /newroot
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
    mkdir -p "$WORK_DIR"
    dd if=/dev/zero of="$ESP_IMG" bs=1K count=4096 status=none
    mkfs.vfat -n "TINEXUS_EFI" "$ESP_IMG" >/dev/null
    mmd -i "$ESP_IMG" ::/EFI
    mmd -i "$ESP_IMG" ::/EFI/BOOT
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
        "$ISO_TREE"

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
    [ -f "$ROOTFS_DIR/usr/share/tinexus-settings/qml/MainWindow.qml" ] || fatal "MainWindow.qml missing from rootfs."
    [ -e "$ROOTFS_DIR/usr/lib/x86_64-linux-gnu/libtinexus_common.so.0" ] || [ -e "$ROOTFS_DIR/usr/lib/libtinexus_common.so.0" ] || fatal "CRITICAL ERROR: libtinexus_common.so.0 missing from rootfs in validate_iso!"

    # Validate ISO vmlinuz version synchronization with rootfs modules
    local actual_iso_kver
    actual_iso_kver="$(file -b "$ISO_TREE/boot/vmlinuz" | sed -n 's/.*version \([^ ]*\).*/\1/p')"
    [ -n "$actual_iso_kver" ] || fatal "Could not extract kernel version from $ISO_TREE/boot/vmlinuz"
    [ "$actual_iso_kver" = "$KVER" ] || fatal "CRITICAL ERROR: Staged ISO vmlinuz version ($actual_iso_kver) does not match build KVER ($KVER)!"
    [ -d "$ROOTFS_DIR/lib/modules/$actual_iso_kver" ] || fatal "CRITICAL ERROR: rootfs/lib/modules/$actual_iso_kver does NOT exist for kernel $actual_iso_kver!"
    info "Verified ISO vmlinuz version ($actual_iso_kver) matches rootfs modules."

    # Verify ISO volume label
    local vol_id
    vol_id="$(blkid -s LABEL -o value "$OUTPUT_ISO" 2>/dev/null || true)"
    if [ -z "$vol_id" ]; then
        vol_id=$(xorriso -indev "$OUTPUT_ISO" -pvd_info 2>&1 | grep -i "Volume Id" | head -n 1 | awk -F"'" '{print $2}' || true)
    fi
    info "Detected ISO Volume Label: '$vol_id'"
    [ "$vol_id" = "TINEXUS_LIVE" ] || warn "Volume label is '$vol_id' (expected TINEXUS_LIVE)"

    # Verify kernel module vermagic inside the generated initramfs
    info "Verifying initramfs kernel module vermagic against kernel ($KVER)..."
    local test_vmag_dir="$WORK_DIR/vmag_check"
    mkdir -p "$test_vmag_dir"
    (cd "$test_vmag_dir" && gzip -dc "$ISO_TREE/boot/initramfs.img" | cpio -idmv "lib/modules/*" >/dev/null 2>&1) || true
    local sample_mod
    sample_mod="$(find "$test_vmag_dir" -name "isofs.ko*" | head -n 1)"
    if [ -n "$sample_mod" ]; then
        local ivmag
        ivmag="$(modinfo -F vermagic "$sample_mod" 2>/dev/null | awk '{print $1}')"
        info "Initramfs isofs module vermagic: $ivmag"
        [ "$ivmag" = "$KVER" ] || fatal "CRITICAL ERROR: Initramfs module vermagic ($ivmag) does not match kernel ($KVER)!"
    else
        warn "isofs.ko not found in initramfs for verification."
    fi
    # Verify audio driver modprobe priority configuration
    info "Verifying audio driver modprobe.d priority configuration in rootfs..."
    [ -f "$ROOTFS_DIR/etc/modprobe.d/sof-priority.conf" ] || fatal "CRITICAL ERROR: $ROOTFS_DIR/etc/modprobe.d/sof-priority.conf is missing!"
    grep -q "dsp_driver=0" "$ROOTFS_DIR/etc/modprobe.d/sof-priority.conf" || fatal "CRITICAL ERROR: dsp_driver=0 missing in rootfs sof-priority.conf!"
    success "Verified /etc/modprobe.d/sof-priority.conf is correctly staged with dsp_driver=0 (universal auto-detect)."

    (cd "$BUILD_DIR" && sha256sum "$(basename "$OUTPUT_ISO")" > "$(basename "$OUTPUT_ISO").sha256")
    
    success "All checks passed! The ISO is a true BIOS+UEFI Hybrid with matching kernel/module vermagic."
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
