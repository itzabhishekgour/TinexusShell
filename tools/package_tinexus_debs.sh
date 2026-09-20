#!/usr/bin/env bash
# ==============================================================================
# Tinexus Platform — First-Party Debian (.deb) Package Builde
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="${PROJECT_DIR:-$(dirname "$SCRIPT_DIR")}"
BUILD_DIR="${BUILD_DIR:-$PROJECT_DIR/build}"
BIN_DIR="${BIN_DIR:-$BUILD_DIR/bin}"
LIB_DIR="${LIB_DIR:-$BUILD_DIR/lib}"
OUT_DEB_DIR="${1:-${OUT_DEB_DIR:-$BUILD_DIR/debs}}"
VERSION="${VERSION:-1.0.0}"

info()    { echo -e "\e[1;34m[INFO]\e[0m $1"; }
success() { echo -e "\e[1;32m[SUCCESS]\e[0m $1"; }
fatal()   { echo -e "\e[1;31m[FATAL]\e[0m $1"; exit 1; }

mkdir -p "$OUT_DEB_DIR"
WORK_DIR=$(mktemp -d /tmp/tinexus_debs_XXXXXX)
cleanup() {
    rm -rf "$WORK_DIR"
}
trap cleanup EXIT

# ── Pre-Packaging Sanity Verification ─────────────────────────────────────────
info "Running strict pre-packaging sanity checks on UI binaries..."
for b in tinexus-shell tinexus-dock tinexus-launcher; do
    bin_path="$BIN_DIR/$b"
    [ -f "$bin_path" ] || fatal "Pre-packaging sanity check FAILED: $b binary not found in $BIN_DIR!"
    
    txui_count=$(strings "$bin_path" | grep -E "txui::|N4txui" -c || true)
    if [ "$txui_count" -ne 0 ]; then
        fatal "CRITICAL SANITY FAILURE: $b contains $txui_count txui references! Legacy txui binary detected. Packaging aborted."
    fi
    
    if ! strings "$bin_path" | grep "libQt6Core" >/dev/null 2>&1; then
        fatal "CRITICAL SANITY FAILURE: $b is NOT linked against Qt6! Packaging aborted."
    fi
    success "Sanity check passed for $b (0 txui references, Qt6 confirmed)."
done

# ── 1. Package: tinexus-core ──────────────────────────────────────────────────
info "Building tinexus-core_${VERSION}_amd64.deb..."
CORE_DIR="$WORK_DIR/tinexus-core"
mkdir -p "$CORE_DIR/DEBIAN" "$CORE_DIR/usr/bin" "$CORE_DIR/usr/lib/x86_64-linux-gnu" \
         "$CORE_DIR/etc/modprobe.d" "$CORE_DIR/etc/dbus-1/system.d" "$CORE_DIR/etc/udev/rules.d"

cat > "$CORE_DIR/DEBIAN/control" << EOF
Package: tinexus-core
Version: ${VERSION}
Section: base
Priority: required
Architecture: amd64
Depends: dbus, udev, kmod, alsa-utils, libc6 (>= 2.38)
Maintainer: Tinexus Engineering Team <team@tinexus.org>
Description: Tinexus Platform Core Supervisor and IPC Broke
 Provides PID 1 runtime manager (tinexus-serviced), central IPC broke
 (tinexus-ipcd), desktop session supervisor (tinexus-session), and core
 shared libraries.
EOF

# Copy binaries
for b in tinexus-serviced tinexus-ipcd tinexus-session tinexus-searchd tinexus-hardware-probe tinexus-wifi tx-appimage; do
    if [ -f "$BIN_DIR/$b" ]; then
        cp -L "$BIN_DIR/$b" "$CORE_DIR/usr/bin/"
        chmod 0755 "$CORE_DIR/usr/bin/$b"
    elif [ -f "$BUILD_DIR/$b" ]; then
        cp -L "$BUILD_DIR/$b" "$CORE_DIR/usr/bin/"
        chmod 0755 "$CORE_DIR/usr/bin/$b"
    fi
done
[ -f "$CORE_DIR/usr/bin/tinexus-serviced" ] || fatal "tinexus-serviced missing from $BIN_DIR"

# Copy shared libraries
mkdir -p "$CORE_DIR/usr/lib/x86_64-linux-gnu" "$CORE_DIR/usr/lib"
for search_dir in "$LIB_DIR" "$BUILD_DIR/lib" "$PROJECT_DIR/build/lib"; do
    if [ -d "$search_dir" ]; then
        find -L "$search_dir" -maxdepth 1 \( -type f -o -type l \) -name "libtinexus*.so*" | while read -r libfile; do
            cp -a "$libfile" "$CORE_DIR/usr/lib/x86_64-linux-gnu/" 2>/dev/null || true
            cp -a "$libfile" "$CORE_DIR/usr/lib/" 2>/dev/null || true
        done
    fi
done

# Ensure ld.so configuration file for Tinexus libraries
mkdir -p "$CORE_DIR/etc/ld.so.conf.d"
cat << 'EOF_LD' > "$CORE_DIR/etc/ld.so.conf.d/tinexus.conf"
/usr/lib/x86_64-linux-gnu
/usr/lib
/usr/local/lib
EOF_LD

# Add postinst to run ldconfig upon package installation
cat << 'EOF_POSTINST' > "$CORE_DIR/DEBIAN/postinst"
#!/bin/sh
set -e
if [ "$1" = "configure" ]; then
    if command -v ldconfig >/dev/null 2>&1; then
        ldconfig
    fi
fi
exit 0
EOF_POSTINST
chmod 0755 "$CORE_DIR/DEBIAN/postinst"

# Strict verification: Ensure libtinexus_common.so.0 is packaged
if [ ! -e "$CORE_DIR/usr/lib/x86_64-linux-gnu/libtinexus_common.so.0" ] && [ ! -e "$CORE_DIR/usr/lib/libtinexus_common.so.0" ]; then
    fatal "CRITICAL ERROR: libtinexus_common.so.0 was NOT packaged into tinexus-core!"
fi
info "Staged shared libraries in tinexus-core: $(ls "$CORE_DIR/usr/lib/x86_64-linux-gnu"/libtinexus*.so* 2>/dev/null | xargs -n1 basename | tr '\n' ' ')"

# Configuration files
cat << 'EOF' > "$CORE_DIR/etc/modprobe.d/sof-priority.conf"
options snd-intel-dspcfg dsp_driver=0
softdep snd_hda_intel pre: snd_sof_pci_intel_cnl snd_sof_pci_intel_icl snd_sof_pci_intel_tgl snd_sof_pci_intel_mtl snd_sof_intel_hda_generic
softdep snd_sof_intel_hda_generic pre: snd_soc_hdac_hda snd_soc_skl_hda_dsp
EOF

cat << 'EOF' > "$CORE_DIR/etc/dbus-1/system.d/tinexus-logind.conf"
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
EOF

dpkg-deb --build "$CORE_DIR" "$OUT_DEB_DIR/tinexus-core_${VERSION}_amd64.deb"
success "Built tinexus-core_${VERSION}_amd64.deb"

# ── 2. Package: tinexus-compositor ────────────────────────────────────────────
info "Building tinexus-compositor_${VERSION}_amd64.deb..."
COMP_DIR="$WORK_DIR/tinexus-compositor"
mkdir -p "$COMP_DIR/DEBIAN" "$COMP_DIR/usr/bin" "$COMP_DIR/etc/udev/rules.d"

cat > "$COMP_DIR/DEBIAN/control" << EOF
Package: tinexus-compositor
Version: ${VERSION}
Section: x11
Priority: required
Architecture: amd64
Depends: tinexus-core (= ${VERSION}), libwayland-server0, libxkbcommon0, libpixman-1-0, libinput10, libseat1, libegl-mesa0, libgbm1, libwlroots-0.19, libliftoff0, libxcb-errors0, libxcb-ewmh2, libxcb-icccm4, xdg-desktop-portal-wlr, pipewire, pipewire-pulse, pipewire-alsa, pulseaudio-utils, libasound2-plugins, grim, xwayland
Maintainer: Tinexus Engineering Team <team@tinexus.org>
Description: Tinexus Wayland Compositor (wlroots based)
 Provides tinexus-comp compositor, hardware probe tools, and early splash.
EOF

for b in tinexus-comp tinexus-splash tinexus-comp-inspector tinexus-drm-info; do
    if [ -f "$BIN_DIR/$b" ]; then
        cp -L "$BIN_DIR/$b" "$COMP_DIR/usr/bin/"
        chmod 0755 "$COMP_DIR/usr/bin/$b"
    elif [ -f "$BUILD_DIR/$b" ]; then
        cp -L "$BUILD_DIR/$b" "$COMP_DIR/usr/bin/"
        chmod 0755 "$COMP_DIR/usr/bin/$b"
    fi
done
[ -f "$COMP_DIR/usr/bin/tinexus-comp" ] || fatal "tinexus-comp missing from $BIN_DIR"

if [ -f "$PROJECT_DIR/assets/logo/tinexus-logo.png" ]; then
    cp -L "$PROJECT_DIR/assets/logo/tinexus-logo.png" "$COMP_DIR/tinexus-logo.png"
fi

# Stage portal configuration for xdg-desktop-portal
mkdir -p "$COMP_DIR/usr/share/xdg-desktop-portal"
if [ -f "$PROJECT_DIR/assets/portals/tinexus-portals.conf" ]; then
    cp -L "$PROJECT_DIR/assets/portals/tinexus-portals.conf" "$COMP_DIR/usr/share/xdg-desktop-portal/tinexus-portals.conf"
    chmod 0644 "$COMP_DIR/usr/share/xdg-desktop-portal/tinexus-portals.conf"
fi

cat << 'EOF' > "$COMP_DIR/etc/udev/rules.d/99-tinexus-seat.rules"
SUBSYSTEM=="input", ENV{ID_INPUT}=="1", ENV{ID_SEAT}="seat0", TAG+="seat", TAG+="seat0", TAG+="uaccess"
SUBSYSTEM=="drm", KERNEL=="card[0-9]*", ENV{ID_SEAT}="seat0", TAG+="seat", TAG+="seat0", TAG+="master-of-seat", TAG+="uaccess"
EOF

dpkg-deb --build "$COMP_DIR" "$OUT_DEB_DIR/tinexus-compositor_${VERSION}_amd64.deb"
success "Built tinexus-compositor_${VERSION}_amd64.deb"

# ── 3. Package: tinexus-desktop ───────────────────────────────────────────────
info "Building tinexus-desktop_${VERSION}_amd64.deb..."
DESK_DIR="$WORK_DIR/tinexus-desktop"
mkdir -p "$DESK_DIR/DEBIAN" "$DESK_DIR/usr/bin" "$DESK_DIR/usr/share/tinexus/fonts" \
         "$DESK_DIR/usr/share/backgrounds" "$DESK_DIR/usr/share/icons/hicolor/scalable/apps" \
         "$DESK_DIR/usr/share/pixmaps" "$DESK_DIR/etc/pam.d"

cat > "$DESK_DIR/DEBIAN/control" << EOF
Package: tinexus-desktop
Version: ${VERSION}
Section: x11
Priority: required
Architecture: amd64
Depends: tinexus-core (= ${VERSION}), tinexus-compositor (= ${VERSION}), qt6-wayland, qml6-module-qtquick, liblayershellqtinterface6, layer-shell-qt, foot, fonts-dejavu-core
Maintainer: Tinexus Engineering Team <team@tinexus.org>
Description: Tinexus Shell, Dock, Launcher, Lock, and Wallpaper Environment
 Provides the Qt6/QML desktop shell components:
  - tinexus-shell: AuraNotch status bar, notification center, control center
  - tinexus-dock: floating application dock
  - tinexus-launcher: Ctrl+K command palette
  - tinexus-lock: PAM-authenticated layer-shell lock screen
  - tinexus-wallpaper: wallpaper renderer and dynamic solar schedule
EOF

# Copy binaries
for b in tinexus-shell tinexus-dock tinexus-launcher tinexus-lock tinexus-wallpaper tinexus-notifications tinexus-terminal tinexus-files tinexus-settings-ui tinexus-monitor tinexus-app-installer; do
    if [ -f "$BIN_DIR/$b" ]; then
        cp -L "$BIN_DIR/$b" "$DESK_DIR/usr/bin/"
        chmod 0755 "$DESK_DIR/usr/bin/$b"
    elif [ -f "$BUILD_DIR/$b" ]; then
        cp -L "$BUILD_DIR/$b" "$DESK_DIR/usr/bin/"
        chmod 0755 "$DESK_DIR/usr/bin/$b"
    fi
done
ln -sf tinexus-settings-ui "$DESK_DIR/usr/bin/tinexus-settings"

# Copy QML components
for comp in common dock shell launcher lock settings-ui; do
    if [ -d "$PROJECT_DIR/src/$comp/qml" ]; then
        mkdir -p "$DESK_DIR/usr/share/tinexus/$comp/qml"
        cp -r "$PROJECT_DIR/src/$comp/qml/"* "$DESK_DIR/usr/share/tinexus/$comp/qml/"
        mkdir -p "$DESK_DIR/usr/share/tinexus-$comp/qml"
        cp -r "$PROJECT_DIR/src/$comp/qml/"* "$DESK_DIR/usr/share/tinexus-$comp/qml/"
        if [ "$comp" = "settings-ui" ]; then
            mkdir -p "$DESK_DIR/usr/share/tinexus-settings/qml"
            cp -r "$PROJECT_DIR/src/$comp/qml/"* "$DESK_DIR/usr/share/tinexus-settings/qml/"
        fi
    fi
done

# Stage udev rules (backlight permissions & rfkill)
mkdir -p "$DESK_DIR/etc/udev/rules.d"
if [ -d "$PROJECT_DIR/data/udev" ]; then
    cp -f "$PROJECT_DIR/data/udev/"*.rules "$DESK_DIR/etc/udev/rules.d/" 2>/dev/null || true
    chmod 0644 "$DESK_DIR/etc/udev/rules.d/"*.rules 2>/dev/null || true
fi

# Stage polkit authorization rules (poweroff, reboot, suspend)
mkdir -p "$DESK_DIR/etc/polkit-1/rules.d"
if [ -d "$PROJECT_DIR/data/polkit" ]; then
    cp -f "$PROJECT_DIR/data/polkit/"*.rules "$DESK_DIR/etc/polkit-1/rules.d/" 2>/dev/null || true
    chmod 0755 "$DESK_DIR/etc/polkit-1/rules.d"
    chmod 0644 "$DESK_DIR/etc/polkit-1/rules.d/"*.rules 2>/dev/null || true
fi

# Fonts
if [ -f "$PROJECT_DIR/assets/fonts/Inter-Regular.ttf" ]; then
    mkdir -p "$DESK_DIR/usr/share/fonts/truetype/inter" "$DESK_DIR/usr/share/tinexus/fonts"
    cp "$PROJECT_DIR/assets/fonts/Inter-Regular.ttf" "$DESK_DIR/usr/share/fonts/truetype/inter/"
    cp "$PROJECT_DIR/assets/fonts/Inter-Bold.ttf" "$DESK_DIR/usr/share/fonts/truetype/inter/"
    cp "$PROJECT_DIR/assets/fonts/Inter-Regular.ttf" "$DESK_DIR/usr/share/tinexus/fonts/"
    cp "$PROJECT_DIR/assets/fonts/Inter-Bold.ttf" "$DESK_DIR/usr/share/tinexus/fonts/"
fi

# Wallpapers & Logos
mkdir -p "$DESK_DIR/usr/share/backgrounds" \
         "$DESK_DIR/usr/share/tinexus" \
         "$DESK_DIR/usr/share/pixmaps" \
         "$DESK_DIR/usr/share/icons/hicolor/scalable/apps" \
         "$DESK_DIR/usr/share/icons/hicolor/32x32/apps"

if [ -d "$PROJECT_DIR/assets/wallpaper" ]; then
    cp -L "$PROJECT_DIR"/assets/wallpaper/* "$DESK_DIR/usr/share/backgrounds/" 2>/dev/null || true
fi
if [ -f "$PROJECT_DIR/assets/logo/tinexus-logo.png" ]; then
    cp -L "$PROJECT_DIR/assets/logo/tinexus-logo.png" "$DESK_DIR/usr/share/tinexus/tinexus-logo.png"
    cp -L "$PROJECT_DIR/assets/logo/tinexus-logo.png" "$DESK_DIR/usr/share/pixmaps/tinexus-logo.png"
fi
if [ -f "$PROJECT_DIR/assets/logo/tinexus-logo-32.png" ]; then
    cp -L "$PROJECT_DIR/assets/logo/tinexus-logo-32.png" "$DESK_DIR/usr/share/icons/hicolor/32x32/apps/tinexus-logo.png"
fi
if [ -f "$PROJECT_DIR/assets/logo/tinexus-logo.svg" ]; then
    cp -L "$PROJECT_DIR/assets/logo/tinexus-logo.svg" "$DESK_DIR/usr/share/icons/hicolor/scalable/apps/tinexus-logo.svg"
fi

# System Audio Assets & Sounds
mkdir -p "$DESK_DIR/usr/share/sounds/tinexus"
if [ -d "$PROJECT_DIR/assets/sounds" ]; then
    cp -L "$PROJECT_DIR"/assets/sounds/* "$DESK_DIR/usr/share/sounds/tinexus/" 2>/dev/null || true
fi

# PAM configuration for tinexus-lock
cat << 'EOF' > "$DESK_DIR/etc/pam.d/tinexus-lock"
#%PAM-1.0
auth      sufficient pam_permit.so
account   sufficient pam_permit.so
password  sufficient pam_permit.so
session   sufficient pam_permit.so
auth      include   common-auth
account   include   common-account
EOF

# Desktop and Autostart registration for tinexus-dock
mkdir -p "$DESK_DIR/etc/xdg/autostart" "$DESK_DIR/usr/share/applications"
cat << 'EOF_DOCK_DESKTOP' > "$DESK_DIR/usr/share/applications/tinexus-dock.desktop"
[Desktop Entry]
Name=Tinexus Dock
Comment=Tinexus Wayland Dock
Exec=/usr/bin/tinexus-dock
Icon=user-desktop
Terminal=false
Type=Application
Categories=System;Core;
OnlyShowIn=Tinexus;
EOF_DOCK_DESKTOP
cp -f "$DESK_DIR/usr/share/applications/tinexus-dock.desktop" "$DESK_DIR/etc/xdg/autostart/tinexus-dock.desktop"

dpkg-deb --build "$DESK_DIR" "$OUT_DEB_DIR/tinexus-desktop_${VERSION}_amd64.deb"
success "Built tinexus-desktop_${VERSION}_amd64.deb"

# ── 4. Package: tinexus-apps ──────────────────────────────────────────────────
info "Building tinexus-apps_${VERSION}_amd64.deb..."
APPS_DIR="$WORK_DIR/tinexus-apps"
mkdir -p "$APPS_DIR/DEBIAN" "$APPS_DIR/usr/bin" "$APPS_DIR/usr/share/applications"

cat > "$APPS_DIR/DEBIAN/control" << EOF
Package: tinexus-apps
Version: ${VERSION}
Section: utils
Priority: optional
Architecture: amd64
Depends: tinexus-desktop (= ${VERSION}), calamares, parted, e2fsprogs, dosfstools, squashfs-tools, grub-efi-amd64-bin
Maintainer: Tinexus Engineering Team <team@tinexus.org>
Description: Tinexus Core User Applications
 Includes Settings UI, Activity Monitor, File Manager, About Profiler,
 Terminal Emulator, and App Store.
EOF

for b in tinexus-settings tinexus-settings-ui tinexus-files tinexus-about tinexus-monitor tinexus-store tinexus-terminal; do
    if [ -f "$BIN_DIR/$b" ]; then
        cp -L "$BIN_DIR/$b" "$APPS_DIR/usr/bin/"
        chmod 0755 "$APPS_DIR/usr/bin/$b"
    elif [ -f "$BUILD_DIR/$b" ]; then
        cp -L "$BUILD_DIR/$b" "$APPS_DIR/usr/bin/"
        chmod 0755 "$APPS_DIR/usr/bin/$b"
    fi
done
ln -sf tinexus-settings-ui "$APPS_DIR/usr/bin/tinexus-settings"

# Copy app QML directories
for app in settings-ui about monitor files; do
    if [ -d "$PROJECT_DIR/src/$app/qml" ]; then
        target_name="tinexus-$app"
        [ "$app" = "settings-ui" ] && target_name="tinexus-settings"
        mkdir -p "$APPS_DIR/usr/share/$target_name/qml"
        cp -r "$PROJECT_DIR/src/$app/qml/"* "$APPS_DIR/usr/share/$target_name/qml/"
        mkdir -p "$APPS_DIR/usr/share/tinexus/$app/qml"
        cp -r "$PROJECT_DIR/src/$app/qml/"* "$APPS_DIR/usr/share/tinexus/$app/qml/"
        if [ "$app" = "settings-ui" ]; then
            mkdir -p "$APPS_DIR/usr/share/tinexus-settings-ui/qml"
            cp -r "$PROJECT_DIR/src/$app/qml/"* "$APPS_DIR/usr/share/tinexus-settings-ui/qml/"
        fi
    fi
done

# Generate .desktop application launcher entries
cat > "$APPS_DIR/usr/share/applications/tinexus-store.desktop" << 'EOF'
[Desktop Entry]
Name=App Store
Comment=Discover and install Linux applications
Exec=/usr/bin/tinexus-store
Icon=system-software-install
Terminal=false
Type=Application
Categories=System;Utility;PackageManager;
EOF
ln -sf tinexus-store.desktop "$APPS_DIR/usr/share/applications/tinexus-app-installer.desktop" 2>/dev/null || true

cat > "$APPS_DIR/usr/share/applications/tinexus-files.desktop" << 'EOF'
[Desktop Entry]
Name=Files
Comment=Tinexus File Manage
Exec=/usr/bin/tinexus-files
Icon=system-file-manage
Terminal=false
Type=Application
Categories=System;Utility;Core;
EOF

cat > "$APPS_DIR/usr/share/applications/tinexus-about.desktop" << 'EOF'
[Desktop Entry]
Name=About Tinexus
Comment=System Profiler and Hardware Specifications
Exec=/usr/bin/tinexus-about
Icon=tinexus-logo
Terminal=false
Type=Application
Categories=System;Core;
EOF

cat > "$APPS_DIR/usr/share/applications/tinexus-monitor.desktop" << 'EOF'
[Desktop Entry]
Name=Activity Monito
Comment=Platform Resource and Process Monito
Exec=/usr/bin/tinexus-monito
Icon=utilities-system-monito
Terminal=false
Type=Application
Categories=System;Monitor;Core;
EOF

cat > "$APPS_DIR/usr/share/applications/tinexus-settings.desktop" << 'EOF'
[Desktop Entry]
Name=Settings
Comment=Tinexus System Settings
Exec=/usr/bin/tinexus-settings-ui
Icon=preferences-system
Terminal=false
Type=Application
Categories=System;Settings;
EOF

cat > "$APPS_DIR/usr/share/applications/tinexus-terminal.desktop" << 'EOF'
[Desktop Entry]
Name=Tinexus Terminal
Comment=Default GPU Wayland Terminal
Exec=/usr/bin/tinexus-terminal
Icon=utilities-terminal
Terminal=false
Type=Application
Categories=System;TerminalEmulator;Core;
EOF

cat > "$APPS_DIR/usr/share/applications/foot.desktop" << 'EOF'
[Desktop Entry]
Name=Foot Terminal
Comment=Lightweight Wayland Terminal Emulato
Exec=/usr/bin/foot
Icon=utilities-terminal
Terminal=false
Type=Application
Categories=System;TerminalEmulator;Core;
EOF

cat > "$APPS_DIR/usr/share/applications/tinexus-installer.desktop" << 'EOF'
[Desktop Entry]
Name=Install Tinexus OS
GenericName=Live System Installer
Comment=Install Tinexus OS permanently to your storage drive
Exec=sudo -E calamares -d
Icon=system-software-install
Terminal=false
Type=Application
Categories=System;Utility;Core;
StartupNotify=true
EOF

dpkg-deb --build "$APPS_DIR" "$OUT_DEB_DIR/tinexus-apps_${VERSION}_amd64.deb"
success "Built tinexus-apps_${VERSION}_amd64.deb"

echo ""
success "All Tinexus Debian packages generated in $OUT_DEB_DIR:"
ls -lh "$OUT_DEB_DIR"/*.deb
