#!/bin/bash
# ==============================================================================
# Tinexus Platform — Rebuild Desktop ISO with Layer-Shell-Qt, Firefox & Network Fix
# ==============================================================================
set -euo pipefail

hwclock -s 2>/dev/null || true

PROJECT_DIR="/workspace"
BUILD_DIR="$PROJECT_DIR/build"
ORIG_ISO="$BUILD_DIR/Tinexus-x86_64.iso"
NEW_ISO="$BUILD_DIR/Tinexus-x86_64-updated.iso"

if [ -f "$BUILD_DIR/workdisk.img" ]; then
    mkdir -p /mnt/workdisk
    mountpoint -q /mnt/workdisk || mount -o loop "$BUILD_DIR/workdisk.img" /mnt/workdisk 2>/dev/null || true
fi

if mountpoint -q /mnt/workdisk; then
    UPPER_DIR="/mnt/workdisk/upper"
    WORK_DIR="/mnt/workdisk/work"
    NEW_SQUASHFS="/mnt/workdisk/rootfs.squashfs"
else
    UPPER_DIR="/dev/shm/upper"
    WORK_DIR="/dev/shm/work"
    NEW_SQUASHFS="/dev/shm/rootfs.squashfs"
fi

info()    { echo -e "\e[1;34m[INFO]\e[0m $1"; }
success() { echo -e "\e[1;32m[SUCCESS]\e[0m $1"; }
warn()    { echo -e "\e[1;33m[WARN]\e[0m $1"; }
fatal()   { echo -e "\e[1;31m[FATAL]\e[0m $1"; exit 1; }

info "Checking requirements..."
[ -f "$ORIG_ISO" ] || fatal "Original ISO not found at $ORIG_ISO"
command -v mksquashfs >/dev/null 2>&1 || fatal "mksquashfs not found"
command -v xorriso >/dev/null 2>&1 || fatal "xorriso not found"

mkdir -p /mnt/isomnt /mnt/squashfs /mnt/rootfs "$UPPER_DIR" "$WORK_DIR"

cleanup_mounts() {
    info "Running cleanup handler..."
    fuser -km /mnt/rootfs 2>/dev/null || true
    for m in /mnt/rootfs/dev/pts /mnt/rootfs/dev /mnt/rootfs/workspace /mnt/rootfs/proc /mnt/rootfs/sys /mnt/rootfs/run /mnt/rootfs/tmp /mnt/rootfs /mnt/squashfs /mnt/isomnt; do
        while mount | grep " $m " >/dev/null 2>&1; do
            umount "$m" 2>/dev/null || umount -l "$m" 2>/dev/null || break
        done
    done
    rm -rf "$UPPER_DIR" "$WORK_DIR" 2>/dev/null || true
}
trap cleanup_mounts EXIT

# 1. Mount original ISO if not mounted
if ! mountpoint -q /mnt/isomnt; then
    info "Mounting original ISO to /mnt/isomnt..."
    mount -o loop,ro "$ORIG_ISO" /mnt/isomnt
fi

# 2. Mount original squashfs if not mounted
if ! mountpoint -q /mnt/squashfs; then
    info "Mounting squashfs to /mnt/squashfs..."
    mount -o loop,ro /mnt/isomnt/live/rootfs.squashfs /mnt/squashfs
fi

# 3. Mount overlayfs on /mnt/rootfs
if ! mountpoint -q /mnt/rootfs; then
    info "Mounting overlayfs to /mnt/rootfs..."
    mount -t overlay overlay -o lowerdir=/mnt/squashfs,upperdir="$UPPER_DIR",workdir="$WORK_DIR" /mnt/rootfs
fi

# 4. Setup chroot bindings
info "Setting up chroot bindings..."
echo "nameserver 8.8.8.8" > /mnt/rootfs/etc/resolv.conf
mountpoint -q /mnt/rootfs/proc || mount --bind /proc /mnt/rootfs/proc 2>/dev/null || true
mountpoint -q /mnt/rootfs/sys || mount --bind /sys /mnt/rootfs/sys 2>/dev/null || true
mountpoint -q /mnt/rootfs/dev || mount --bind /dev /mnt/rootfs/dev 2>/dev/null || true
mkdir -p /mnt/rootfs/workspace
mountpoint -q /mnt/rootfs/workspace || mount --bind /workspace /mnt/rootfs/workspace 2>/dev/null || true

# Ensure temporary directories with proper sticky bit permissions
if mountpoint -q /mnt/workdisk; then
    mkdir -p /mnt/workdisk/tmp
    chmod 1777 /mnt/workdisk/tmp
    mountpoint -q /mnt/rootfs/tmp || mount --bind /mnt/workdisk/tmp /mnt/rootfs/tmp 2>/dev/null || true
fi
mkdir -p /mnt/rootfs/tmp /mnt/rootfs/var/tmp /mnt/rootfs/tmp/build_apps
chmod 1777 /mnt/rootfs/tmp /mnt/rootfs/var/tmp /mnt/rootfs/tmp/build_apps 2>/dev/null || true

# 5. In chroot: Install core dependencies & build tools
info "Checking / Installing dependencies and dev packages in rootfs..."
chroot /mnt/rootfs /usr/bin/apt-get update || true
chroot /mnt/rootfs /usr/bin/apt-get install -y --no-install-recommends \
    g++ qt6-base-dev-tools qt6-declarative-dev qml6-module-qtquick-shapes liblayershellqtinterface-dev layer-shell-qt \
    isc-dhcp-client procps libcap2-bin wget ca-certificates libssl-dev libwayland-dev libpixman-1-dev libfreetype-dev \
    libwlroots-0.19-dev libdrm-dev libvulkan-dev libxkbcommon-dev libpam0g-dev \
    systemd systemd-sysv libpam-systemd calamares calamares-settings-ubuntu-common \
    linux-firmware shim-signed grub-efi-amd64-signed libnotify-bin

[ -f /mnt/rootfs/usr/lib/x86_64-linux-gnu/qt6/plugins/wayland-shell-integration/liblayer-shell.so ] || fatal "liblayer-shell.so missing after install!"
[ -f /mnt/rootfs/sbin/dhclient ] || [ -f /mnt/rootfs/usr/sbin/dhclient ] || fatal "dhclient missing after install!"
[ -f /mnt/rootfs/usr/bin/pkill ] || [ -f /mnt/rootfs/bin/pkill ] || fatal "pkill missing after install!"
success "Verified layer-shell-qt, isc-dhcp-client, procps, and libcap2-bin are present."

# ── 6. Mozilla Firefox Installation (Native Non-Snap DEB) ─────────────────────
if [ ! -f /mnt/rootfs/usr/bin/firefox ]; then
    info "Configuring Mozilla official APT repository for native deb Firefox..."
    mkdir -p /mnt/rootfs/etc/apt/keyrings
    wget -q https://packages.mozilla.org/apt/repo-signing-key.gpg -O /mnt/rootfs/etc/apt/keyrings/packages.mozilla.org.asc || true
    cat > /mnt/rootfs/etc/apt/sources.list.d/mozilla.list << 'EOF'
deb [signed-by=/etc/apt/keyrings/packages.mozilla.org.asc] https://packages.mozilla.org/apt mozilla main
EOF
    cat > /mnt/rootfs/etc/apt/preferences.d/mozilla << 'EOF'
Package: *
Pin: origin packages.mozilla.org
Pin-Priority: 1000
EOF

    info "Installing Firefox via Mozilla APT repo..."
    chroot /mnt/rootfs /usr/bin/apt-get update -o Acquire::Retries=3 || true
    chroot /mnt/rootfs /usr/bin/apt-get install -y --no-install-recommends firefox || true
fi

if [ -f /mnt/rootfs/usr/bin/firefox ]; then
    success "Firefox native DEB confirmed installed in rootfs."
else
    warn "Firefox binary not found in rootfs, proceeding with desktop build."
fi

# ── 7. Configure Firefox Profile with macOS Traffic Lights & Dark Theme ────────
info "Configuring Firefox Wayland profile and Tinexus macOS traffic lights userChrome..."
FF_PROFILE="/mnt/rootfs/etc/skel/.mozilla/firefox/tinexus.default"
mkdir -p "$FF_PROFILE/chrome"

cat > "$FF_PROFILE/user.js" << 'EOF_USERJS'
user_pref("toolkit.legacyUserProfileCustomizations.stylesheets", true);
user_pref("gfx.webrender.enabled", true);
user_pref("media.ffmpeg.vaapi.enabled", true);
user_pref("browser.uidensity", 1);
user_pref("ui.systemUsesDarkTheme", 1);
user_pref("browser.theme.toolbar-theme", 0);
user_pref("browser.theme.content-theme", 0);
user_pref("extensions.activeThemeID", "firefox-compact-dark@mozilla.org");
user_pref("browser.in-content.dark-mode", true);
user_pref("browser.tabs.inTitlebar", 1);
user_pref("browser.tabs.drawInTitlebar", true);
user_pref("datareporting.healthreport.uploadEnabled", false);
user_pref("datareporting.policy.dataSubmissionEnabled", false);
EOF_USERJS

cat > "$FF_PROFILE/chrome/userChrome.css" << 'EOF_CHROME'
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

cat > "/mnt/rootfs/etc/skel/.mozilla/firefox/profiles.ini" << 'EOF_PROFILES'
[Profile0]
Name=tinexus
IsRelative=1
Path=tinexus.default
Default=1

[General]
StartWithLastProfile=1
Version=2
EOF_PROFILES

cat > "/mnt/rootfs/etc/skel/.mozilla/firefox/installs.ini" << 'EOF_INSTALLS'
[Install]
Default=tinexus.default
Locked=1
EOF_INSTALLS

# System-wide Firefox preferences (enforcing dark mode on ALL profiles)
mkdir -p /mnt/rootfs/etc/firefox /mnt/rootfs/usr/lib/firefox/browser/defaults/preferences
cat > /mnt/rootfs/etc/firefox/firefox.js << 'EOF_SYSFF'
pref("ui.systemUsesDarkTheme", 1);
pref("browser.theme.toolbar-theme", 0);
pref("browser.theme.content-theme", 0);
pref("extensions.activeThemeID", "firefox-compact-dark@mozilla.org");
pref("browser.in-content.dark-mode", true);
pref("toolkit.legacyUserProfileCustomizations.stylesheets", true);
EOF_SYSFF
cp -f /mnt/rootfs/etc/firefox/firefox.js /mnt/rootfs/usr/lib/firefox/browser/defaults/preferences/syspref.js 2>/dev/null || true

# Stage into /root/.mozilla and /home/*/.mozilla for live session
mkdir -p /mnt/rootfs/root/.mozilla
cp -r /mnt/rootfs/etc/skel/.mozilla/* /mnt/rootfs/root/.mozilla/ 2>/dev/null || true
if [ -d /mnt/rootfs/home/tinexus ]; then
    mkdir -p /mnt/rootfs/home/tinexus/.mozilla
    cp -r /mnt/rootfs/etc/skel/.mozilla/* /mnt/rootfs/home/tinexus/.mozilla/ 2>/dev/null || true
    chroot /mnt/rootfs chown -R 1000:1000 /home/tinexus/.mozilla 2>/dev/null || true
fi

# Update firefox.desktop with native Wayland execution
mkdir -p /mnt/rootfs/usr/share/applications
cat > "/mnt/rootfs/usr/share/applications/firefox.desktop" << 'EOF_FFDESKTOP'
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
chmod 0644 /mnt/rootfs/usr/share/applications/firefox.desktop
success "Firefox profile, dark theme, and desktop entry staged."

# ── Configure Foot Terminal (Dark Theme & macOS/Wayland Aesthetics) ──────────
info "Configuring Foot terminal dark palette and decorations..."
mkdir -p /mnt/rootfs/etc/xdg/foot /mnt/rootfs/root/.config/foot /mnt/rootfs/etc/skel/.config/foot
cat > /mnt/rootfs/etc/xdg/foot/foot.ini << 'EOF_FOOT'
[main]
term=foot
font=monospace:size=11
pad=14x14

[colors]
alpha=0.92
background=10121a
foreground=e5e7eb
regular0=10121a
regular1=f87171
regular2=34d399
regular3=fbbf24
regular4=60a5fa
regular5=c084fc
regular6=38bdf8
regular7=e5e7eb
bright0=374151
bright1=ef4444
bright2=10b981
bright3=f59e0b
bright4=3b82f6
bright5=a855f7
bright6=06b6d4
bright7=ffffff

[csd]
preferred=none
size=0
border-width=1
border-color=38bdf844
EOF_FOOT

cp -f /mnt/rootfs/etc/xdg/foot/foot.ini /mnt/rootfs/root/.config/foot/foot.ini
cp -f /mnt/rootfs/etc/xdg/foot/foot.ini /mnt/rootfs/etc/skel/.config/foot/foot.ini
if [ -d /mnt/rootfs/home/tinexus ]; then
    mkdir -p /mnt/rootfs/home/tinexus/.config/foot
    cp -f /mnt/rootfs/etc/xdg/foot/foot.ini /mnt/rootfs/home/tinexus/.config/foot/foot.ini
    chroot /mnt/rootfs chown -R 1000:1000 /home/tinexus/.config 2>/dev/null || true
fi
success "Foot terminal dark theme and borderless CSD configured."

# ── 8. Compile Updated Tinexus Binaries ────────────────────────────────────────
info "Compiling updated Tinexus desktop binaries..."
mkdir -p /mnt/rootfs/tmp/build_apps
chmod 1777 /mnt/rootfs/tmp /mnt/rootfs/tmp/build_apps 2>/dev/null || true
MOC_BIN="/usr/lib/qt6/libexec/moc"
QT6_INC="-I/usr/include/x86_64-linux-gnu/qt6 -I/usr/include/x86_64-linux-gnu/qt6/QtCore -I/usr/include/x86_64-linux-gnu/qt6/QtGui -I/usr/include/x86_64-linux-gnu/qt6/QtQuick -I/usr/include/x86_64-linux-gnu/qt6/QtQml -I/usr/include/x86_64-linux-gnu/qt6/QtNetwork -I/usr/include/x86_64-linux-gnu/qt6/QtWaylandClient -I/usr/include/LayerShellQt -I/tmp/build_apps"
COMMON_INC="-I/workspace/include -I/workspace/src -I/workspace/src/common/include -I/workspace/src/ipcd/include"

# 80. libtinexus_common
info "Compiling updated libtinexus_common..."
rm -f /workspace/build/lib/libtinexus_common.so.0.1.0
BUILD_START_COMMON=$(date +%s)
chroot /mnt/rootfs /usr/bin/g++ -std=c++20 -shared -fPIC \
  -I/workspace/include -I/workspace/src -I/workspace/src/common/include -I/usr/include/libdrm \
  /workspace/src/common/AudioUtils.cpp /workspace/src/common/BacklightUtils.cpp /workspace/src/common/DisplayUtils.cpp \
  /workspace/src/common/GraphicsProbe.cpp /workspace/src/common/HardwareConfig.cpp /workspace/src/common/NetUtils.cpp \
  /workspace/src/common/PlatformServices.cpp /workspace/src/common/TinexusLogo.cpp /workspace/src/common/dbus_power.cpp \
  /workspace/src/common/logger.cpp /workspace/src/common/peer_credentials.cpp /workspace/src/common/string_interner.cpp \
  -ldrm -lgbm -lEGL -lsystemd -lpthread \
  -o /workspace/build/lib/libtinexus_common.so.0.1.0
[ -f /workspace/build/lib/libtinexus_common.so.0.1.0 ] || fatal "libtinexus_common compilation FAILED!"
[ "$(stat -c %Y /workspace/build/lib/libtinexus_common.so.0.1.0)" -ge "$BUILD_START_COMMON" ] || fatal "libtinexus_common mtime is older than build start! Stale library."
ln -sf libtinexus_common.so.0.1.0 /workspace/build/lib/libtinexus_common.so
ln -sf libtinexus_common.so.0.1.0 /workspace/build/lib/libtinexus_common.so.0
cp -av /workspace/build/lib/libtinexus_common.so* /mnt/rootfs/usr/lib/x86_64-linux-gnu/ 2>/dev/null || true
cp -av /workspace/build/lib/libtinexus_common.so* /mnt/rootfs/usr/lib/ 2>/dev/null || true
chroot /mnt/rootfs /sbin/ldconfig

# 80a. tinexus-hardware-probe
info "Compiling tinexus-hardware-probe..."
rm -f /workspace/build/bin/tinexus-hardware-probe
BUILD_START_PROBE=$(date +%s)
chroot /mnt/rootfs /usr/bin/g++ -std=c++20 -O2 \
  -I/workspace/include -I/workspace/src -I/workspace/src/common/include -I/usr/include/libdrm \
  /workspace/src/common/hardware_probe_main.cpp \
  -L/workspace/build/lib -L/usr/lib -L/usr/lib/x86_64-linux-gnu -ltinexus_common -ldrm -lgbm -lEGL -lsystemd -lpthread \
  -o /workspace/build/bin/tinexus-hardware-probe
[ -f /workspace/build/bin/tinexus-hardware-probe ] || fatal "tinexus-hardware-probe compilation FAILED!"
[ "$(stat -c %Y /workspace/build/bin/tinexus-hardware-probe)" -ge "$BUILD_START_PROBE" ] || fatal "tinexus-hardware-probe mtime is older than build start! Stale binary."
cp -f /workspace/build/bin/tinexus-hardware-probe /mnt/rootfs/usr/bin/tinexus-hardware-probe
chmod 0755 /mnt/rootfs/usr/bin/tinexus-hardware-probe

# 80b. tinexus-serviced
info "Compiling tinexus-serviced..."
rm -f /workspace/build/bin/tinexus-serviced
BUILD_START_SERVICED=$(date +%s)
chroot /mnt/rootfs /usr/bin/g++ -std=c++20 -O2 \
  -I/workspace/include -I/workspace/src -I/workspace/src/common/include -I/workspace/src/serviced/include -I/workspace/src/ipcd/include -I/workspace/src/guard/include \
  /workspace/src/serviced/daemon_spec.cpp \
  /workspace/src/serviced/dep_graph.cpp \
  /workspace/src/serviced/event_journal.cpp \
  /workspace/src/serviced/heartbeat_watchdog.cpp \
  /workspace/src/serviced/process_manager.cpp \
  /workspace/src/serviced/runtime_socket.cpp \
  /workspace/src/serviced/launch_authority.cpp \
  /workspace/src/serviced/install_handler.cpp \
  /workspace/src/serviced/logind_mimic.cpp \
  /workspace/src/serviced/main.cpp \
  -L/workspace/build/lib -L/usr/lib -L/usr/lib/x86_64-linux-gnu -ltinexus_common -ltinexus-guard -lcrypto -lsystemd -lpthread \
  -o /workspace/build/bin/tinexus-serviced
[ -f /workspace/build/bin/tinexus-serviced ] || fatal "tinexus-serviced compilation FAILED!"
[ "$(stat -c %Y /workspace/build/bin/tinexus-serviced)" -ge "$BUILD_START_SERVICED" ] || fatal "tinexus-serviced mtime is older than build start! Stale binary."

# 8a. tinexus-session
info "Compiling tinexus-session..."
rm -f /workspace/build/bin/tinexus-session
BUILD_START_SESSION=$(date +%s)
chroot /mnt/rootfs /usr/bin/g++ -std=c++20 -O2 \
  -I/workspace/src -I/workspace/src/common/include -I/workspace/src/session/include \
  /workspace/src/session/main.cpp \
  /workspace/src/session/env_bootstrap.cpp \
  /workspace/src/session/autostart_parser.cpp \
  /workspace/src/session/session_manager.cpp \
  -L/usr/lib -L/usr/lib/x86_64-linux-gnu -ltinexus_common -lpthread \
  -o /workspace/build/bin/tinexus-session
[ -f /workspace/build/bin/tinexus-session ] || fatal "tinexus-session compilation FAILED!"
[ "$(stat -c %Y /workspace/build/bin/tinexus-session)" -ge "$BUILD_START_SESSION" ] || fatal "tinexus-session mtime is older than build start! Stale binary."

# 8b. tinexus-launcher
info "Compiling tinexus-launcher via compile_launcher.sh..."
rm -f /workspace/build/bin/tinexus-launcher
/workspace/tools/compile_launcher.sh
[ -f /workspace/build/bin/tinexus-launcher ] || fatal "tinexus-launcher compilation FAILED!"

# 8c. tinexus-settings-ui
info "Compiling tinexus-settings-ui..."
rm -f /workspace/build/bin/tinexus-settings-ui
BUILD_START_SETTINGS=$(date +%s)
chroot /mnt/rootfs "$MOC_BIN" /workspace/src/settings-ui/qt/SettingsBridge.hpp -o /tmp/build_apps/moc_SettingsBridge.cpp
chroot /mnt/rootfs "$MOC_BIN" /workspace/src/settings-ui/include/settings-ui/SettingsAdaptor.hpp -o /tmp/build_apps/moc_SettingsAdaptor.cpp
chroot /mnt/rootfs /usr/bin/g++ -std=c++20 -O2 \
  $QT6_INC $COMMON_INC -I/workspace/src/guard/include -I/workspace/src/settings-ui -I/workspace/src/settings-ui/qt -I/workspace/src/settings-ui/include -I/usr/include/x86_64-linux-gnu/qt6/QtDBus \
  /workspace/src/settings-ui/qt/main_qt.cpp \
  /workspace/src/settings-ui/qt/SettingsBridge.cpp \
  /workspace/src/settings-ui/qt/SettingsAdaptor.cpp \
  /workspace/src/settings-ui/WifiManager.cpp \
  /tmp/build_apps/moc_SettingsBridge.cpp \
  -L/workspace/build/lib -L/usr/lib/x86_64-linux-gnu -ltinexus-guard -ltinexus_common -lQt6Core -lQt6Gui -lQt6Quick -lQt6Qml -lQt6Network -lQt6DBus -lcrypto \
  -o /workspace/build/bin/tinexus-settings-ui
[ -f /workspace/build/bin/tinexus-settings-ui ] || fatal "tinexus-settings-ui compilation FAILED!"
[ "$(stat -c %Y /workspace/build/bin/tinexus-settings-ui)" -ge "$BUILD_START_SETTINGS" ] || fatal "tinexus-settings-ui mtime is older than build start! Stale binary."
strings /workspace/build/bin/tinexus-settings-ui | grep "libQt6Core" >/dev/null 2>&1 || fatal "tinexus-settings-ui not linked to Qt6!"

# 8d. tinexus-dock
info "Compiling tinexus-dock via compile_dock.sh..."
/workspace/tools/compile_dock.sh
[ -f /workspace/build/bin/tinexus-dock ] || fatal "tinexus-dock compilation FAILED!"

# 8e. tinexus-shell
info "Compiling tinexus-shell via compile_shell.sh..."
/workspace/tools/compile_shell.sh
[ -f /workspace/build/bin/tinexus-shell ] || fatal "tinexus-shell compilation FAILED!"

# 8f. Phase 4 daemons & Qt6/QML apps (tinexus-wallpaper, tinexus-notifications, tinexus-terminal)
info "Compiling Phase 4 daemons via compile_phase4_qml.sh..."
/workspace/tools/compile_phase4_qml.sh
for app in tinexus-wallpaper tinexus-notifications tinexus-terminal test-terminal-pty-e2e; do
    [ -f "/workspace/build/debug/bin/$app" ] && cp -f "/workspace/build/debug/bin/$app" /workspace/build/bin/
done
cp -f /workspace/build/bin/tinexus-wallpaper /workspace/build/tinexus-wallpaper 2>/dev/null || true
[ -f /workspace/build/bin/tinexus-wallpaper ] || fatal "tinexus-wallpaper missing from build/bin!"
[ -f /workspace/build/bin/tinexus-notifications ] || fatal "tinexus-notifications missing from build/bin!"
[ -f /workspace/build/bin/tinexus-terminal ] || fatal "tinexus-terminal missing from build/bin!"

# 8g. tinexus-comp
info "Compiling tinexus-comp via compile_comp.sh..."
/workspace/tools/compile_comp.sh
[ -f /workspace/build/bin/tinexus-comp ] || fatal "tinexus-comp compilation FAILED!"

# 8h. tinexus-monitor
info "Compiling tinexus-monitor..."
rm -f /workspace/build/bin/tinexus-monitor
BUILD_START_MON=$(date +%s)
chroot /mnt/rootfs "$MOC_BIN" /workspace/src/monitor/MonitorBridge.hpp -o /tmp/build_apps/moc_MonitorBridge.cpp
MON_SRC="/workspace/src/monitor"
chroot /mnt/rootfs /usr/bin/g++ -std=c++20 -O2 \
  $QT6_INC $COMMON_INC -I/workspace/src/monitor -I/workspace/src/monitor/include \
  $MON_SRC/main.cpp \
  $MON_SRC/MonitorBridge.cpp \
  $MON_SRC/metrics_snapshot.cpp \
  $MON_SRC/cpu_parser.cpp \
  $MON_SRC/memory_parser.cpp \
  $MON_SRC/disk_parser.cpp \
  $MON_SRC/network_parser.cpp \
  $MON_SRC/power_parser.cpp \
  $MON_SRC/gpu_parser.cpp \
  $MON_SRC/process_tree.cpp \
  $MON_SRC/process_controller.cpp \
  $MON_SRC/resource_monitor.cpp \
  /tmp/build_apps/moc_MonitorBridge.cpp \
  -L/workspace/build/lib -L/usr/lib/x86_64-linux-gnu -ltinexus_common -lQt6Core -lQt6Gui -lQt6Quick -lQt6Qml -lQt6Network \
  -o /workspace/build/bin/tinexus-monitor
[ -f /workspace/build/bin/tinexus-monitor ] || fatal "tinexus-monitor compilation FAILED!"
[ "$(stat -c %Y /workspace/build/bin/tinexus-monitor)" -ge "$BUILD_START_MON" ] || fatal "tinexus-monitor mtime is older than build start! Stale binary."
strings /workspace/build/bin/tinexus-monitor | grep "libQt6Core" >/dev/null 2>&1 || fatal "tinexus-monitor not linked to Qt6!"

# 8i. tinexus-files
info "Compiling tinexus-files..."
rm -f /workspace/build/bin/tinexus-files
BUILD_START_FILES=$(date +%s)
chroot /mnt/rootfs "$MOC_BIN" /workspace/src/files/FilesBridge.hpp -o /tmp/build_apps/moc_FilesBridge.cpp
FILES_SRC="/workspace/src/files"
chroot /mnt/rootfs /usr/bin/g++ -std=c++20 -O2 \
  $QT6_INC $COMMON_INC -I/workspace/src/files -I/workspace/src/files/include \
  $FILES_SRC/main.cpp \
  $FILES_SRC/FilesBridge.cpp \
  -L/workspace/build/lib -L/usr/lib/x86_64-linux-gnu -ltinexus_common -lQt6Core -lQt6Gui -lQt6Quick -lQt6Qml -lQt6Network \
  -o /workspace/build/bin/tinexus-files
[ -f /workspace/build/bin/tinexus-files ] || fatal "tinexus-files compilation FAILED!"
[ "$(stat -c %Y /workspace/build/bin/tinexus-files)" -ge "$BUILD_START_FILES" ] || fatal "tinexus-files mtime is older than build start! Stale binary."
strings /workspace/build/bin/tinexus-files | grep "libQt6Core" >/dev/null 2>&1 || fatal "tinexus-files not linked to Qt6!"

# 8j. tinexus-lock
info "Compiling tinexus-lock (Phase 5 M6)..."
rm -f /workspace/build/bin/tinexus-lock
BUILD_START_LOCK=$(date +%s)
chroot /mnt/rootfs "$MOC_BIN" $QT6_INC $COMMON_INC -I/workspace/src/lock -I/workspace/src/lock/include /workspace/src/lock/LockBridge.hpp -o /tmp/build_apps/moc_LockBridge.cpp
chroot /mnt/rootfs /usr/bin/g++ -std=c++20 -O2 \
  $QT6_INC $COMMON_INC -I/workspace/src/lock -I/workspace/src/lock/include -I/usr/include/x86_64-linux-gnu/qt6/QtDBus -I/tmp/build_apps \
  -DTINEXUS_LOCK_HAS_PAM=1 -DHAVE_LAYERSHELL=1 \
  /workspace/src/lock/main.cpp \
  /workspace/src/lock/LockBridge.cpp \
  -L/workspace/build/lib -L/workspace/build -L/usr/lib -L/usr/lib/x86_64-linux-gnu \
  -ltinexus_common -lQt6Core -lQt6Gui -lQt6Quick -lQt6Qml -lQt6Network -lQt6WaylandClient -lLayerShellQtInterface -lQt6DBus -lpam \
  -o /workspace/build/bin/tinexus-lock
[ -f /workspace/build/bin/tinexus-lock ] || fatal "tinexus-lock compilation FAILED!"
[ "$(stat -c %Y /workspace/build/bin/tinexus-lock)" -ge "$BUILD_START_LOCK" ] || fatal "tinexus-lock mtime is older than build start! Stale binary."
strings /workspace/build/bin/tinexus-lock | grep "libQt6Core" >/dev/null 2>&1 || fatal "tinexus-lock not linked to Qt6!"
cp -f /workspace/build/bin/tinexus-lock /workspace/build/tinexus-lock

success "All desktop binaries successfully compiled and deployed to /workspace/build/bin."

# ── 9. Package & Install Debian Packages into Rootfs ──────────────────────────
info "Regenerating Debian packages..."
chroot /mnt/rootfs /usr/bin/bash /workspace/tools/package_tinexus_debs.sh /workspace/build/debs

info "Installing generated Debian packages into rootfs..."
chroot /mnt/rootfs /usr/bin/dpkg -i --force-overwrite \
    /workspace/build/debs/tinexus-core_1.0.0_amd64.deb \
    /workspace/build/debs/tinexus-compositor_1.0.0_amd64.deb \
    /workspace/build/debs/tinexus-desktop_1.0.0_amd64.deb \
    /workspace/build/debs/tinexus-apps_1.0.0_amd64.deb

# Stage Calamares configuration & branding
if [ -d "/workspace/data/calamares" ]; then
    info "Staging Calamares configuration into /etc/calamares..."
    mkdir -p /mnt/rootfs/etc/calamares
    cp -r /workspace/data/calamares/* /mnt/rootfs/etc/calamares/
    chmod -R 0755 /mnt/rootfs/etc/calamares/
fi

# Stage Systemd services & Presets
if [ -d "/workspace/data/systemd" ]; then
    info "Staging systemd unit files and presets into rootfs..."
    mkdir -p /mnt/rootfs/etc/systemd/system /mnt/rootfs/lib/systemd/system-preset /mnt/rootfs/etc/systemd/system-preset
    cp -r /workspace/data/systemd/* /mnt/rootfs/etc/systemd/system/
    if [ -f "/workspace/data/systemd/90-tinexus.preset" ]; then
        cp -f /workspace/data/systemd/90-tinexus.preset /mnt/rootfs/lib/systemd/system-preset/
        cp -f /workspace/data/systemd/90-tinexus.preset /mnt/rootfs/etc/systemd/system-preset/
    fi
    chmod 0644 /mnt/rootfs/etc/systemd/system/*.service /mnt/rootfs/etc/systemd/system/*.target 2>/dev/null || true
    
    # Ensure serial autologin for headless diagnostics, but mask getty@tty1 so it never overlaps the GUI
    info "Configuring serial autologin on ttyS0 and masking getty@tty1 for clean graphical session..."
    rm -rf /mnt/rootfs/etc/systemd/system/getty@tty1.service.d
    mkdir -p /mnt/rootfs/etc/systemd/system/serial-getty@ttyS0.service.d
    cat << 'EOF_TTYS0' > /mnt/rootfs/etc/systemd/system/serial-getty@ttyS0.service.d/autologin.conf
[Service]
ExecStart=
ExecStart=-/sbin/agetty -o '-p -f -- \\u' --autologin tinexus --noclear --keep-baud 115200,38400,9600 %I $TERM
EOF_TTYS0

    info "Ensuring device access groups, QML staging, and enabling Tinexus session units in systemd..."
    mkdir -p /mnt/rootfs/var/lib/systemd/linger
    touch /mnt/rootfs/var/lib/systemd/linger/tinexus
    mkdir -p /mnt/rootfs/etc/sudoers.d
    echo "tinexus ALL=(ALL) NOPASSWD:ALL" > /mnt/rootfs/etc/sudoers.d/tinexus
    chmod 0440 /mnt/rootfs/etc/sudoers.d/tinexus

    mkdir -p /mnt/rootfs/usr/share/tinexus-lock/qml /mnt/rootfs/usr/share/tinexus/lock/qml
    cp -rf /workspace/src/lock/qml/* /mnt/rootfs/usr/share/tinexus-lock/qml/ 2>/dev/null || true
    cp -rf /workspace/src/lock/qml/* /mnt/rootfs/usr/share/tinexus/lock/qml/ 2>/dev/null || true
    mkdir -p /mnt/rootfs/usr/share/tinexus-dock/qml /mnt/rootfs/usr/share/tinexus/dock/qml
    cp -rf /workspace/src/dock/qml/* /mnt/rootfs/usr/share/tinexus-dock/qml/ 2>/dev/null || true
    cp -rf /workspace/src/dock/qml/* /mnt/rootfs/usr/share/tinexus/dock/qml/ 2>/dev/null || true
    mkdir -p /mnt/rootfs/usr/share/tinexus-shell/qml /mnt/rootfs/usr/share/tinexus/shell/qml
    cp -rf /workspace/src/shell/qml/* /mnt/rootfs/usr/share/tinexus-shell/qml/ 2>/dev/null || true
    cp -rf /workspace/src/shell/qml/* /mnt/rootfs/usr/share/tinexus/shell/qml/ 2>/dev/null || true
    mkdir -p /mnt/rootfs/usr/share/tinexus-launcher/qml /mnt/rootfs/usr/share/tinexus/launcher/qml
    cp -rf /workspace/src/launcher/qml/* /mnt/rootfs/usr/share/tinexus-launcher/qml/ 2>/dev/null || true
    cp -rf /workspace/src/launcher/qml/* /mnt/rootfs/usr/share/tinexus/launcher/qml/ 2>/dev/null || true
    mkdir -p /mnt/rootfs/usr/share/tinexus-settings/qml /mnt/rootfs/usr/share/tinexus-settings-ui/qml /mnt/rootfs/usr/share/tinexus/settings-ui/qml
    cp -rf /workspace/src/settings-ui/qml/* /mnt/rootfs/usr/share/tinexus-settings/qml/ 2>/dev/null || true
    cp -rf /workspace/src/settings-ui/qml/* /mnt/rootfs/usr/share/tinexus-settings-ui/qml/ 2>/dev/null || true
    cp -rf /workspace/src/settings-ui/qml/* /mnt/rootfs/usr/share/tinexus/settings-ui/qml/ 2>/dev/null || true
    mkdir -p /mnt/rootfs/usr/share/tinexus/common/qml
    cp -rf /workspace/src/common/qml/* /mnt/rootfs/usr/share/tinexus/common/qml/ 2>/dev/null || true

    mkdir -p /mnt/rootfs/etc/udev/rules.d /mnt/rootfs/etc/polkit-1/rules.d
    if [ -d "/workspace/data/udev" ]; then
        cp -rf /workspace/data/udev/*.rules /mnt/rootfs/etc/udev/rules.d/ 2>/dev/null || true
        chmod 0644 /mnt/rootfs/etc/udev/rules.d/*.rules 2>/dev/null || true
    fi
    if [ -d "/workspace/data/polkit" ]; then
        cp -rf /workspace/data/polkit/*.rules /mnt/rootfs/etc/polkit-1/rules.d/ 2>/dev/null || true
        chmod 0755 /mnt/rootfs/etc/polkit-1/rules.d 2>/dev/null || true
        chmod 0644 /mnt/rootfs/etc/polkit-1/rules.d/*.rules 2>/dev/null || true
    fi

    ln -sf /usr/bin/tinexus-settings-ui /mnt/rootfs/usr/bin/tinexus-settings


    chroot /mnt/rootfs /bin/bash -c "
        groupadd -r -f seat 2>/dev/null || true
        groupadd -r -f netdev 2>/dev/null || true
        usermod -aG video,input,render,seat,sudo,adm,systemd-journal,netdev tinexus 2>/dev/null || true
        systemctl preset-all 2>/dev/null || true
        systemctl enable tinexus-session.target 2>/dev/null || true
        systemctl enable tinexus-splash.service 2>/dev/null || true
        systemctl enable tinexus-hardware-env.service 2>/dev/null || true
        systemctl enable seatd.service 2>/dev/null || true
        systemctl enable tinexus-comp.service 2>/dev/null || true
        systemctl enable tinexus-serviced.service 2>/dev/null || true
        systemctl disable tinexus-shell.service 2>/dev/null || true
        systemctl disable tinexus-dock.service 2>/dev/null || true
        systemctl mask getty@tty1.service 2>/dev/null || true
        systemctl mask getty@tty.service 2>/dev/null || true
        systemctl mask console-getty.service 2>/dev/null || true
        systemctl set-default tinexus-session.target 2>/dev/null || true
    "
    
    # Assert default target is tinexus-session.target
    def_target="$(chroot /mnt/rootfs systemctl get-default 2>/dev/null || true)"
    info "Current systemd default target: $def_target"
    [ "$def_target" = "tinexus-session.target" ] || fatal "Failed to set default target to tinexus-session.target (got $def_target)!"
    
    # Assert all core units are enabled
    for u in tinexus-session.target seatd.service tinexus-hardware-env.service tinexus-comp.service tinexus-serviced.service; do
        is_en="$(chroot /mnt/rootfs systemctl is-enabled "$u" 2>/dev/null || true)"
        [ "$is_en" = "enabled" ] || fatal "Unit $u is NOT enabled (status: $is_en)!"
        success "Verified unit $u is enabled."
    done

    # Assert hardware probe binary exists and is executable in rootfs
    [ -x "/mnt/rootfs/usr/bin/tinexus-hardware-probe" ] || fatal "tinexus-hardware-probe missing or not executable in /mnt/rootfs/usr/bin/!"
    success "Verified tinexus-hardware-probe binary is staged and executable."
fi

# Stage Install Tinexus OS shortcut on user desktop
info "Staging Install Tinexus OS shortcut on live user Desktop..."
mkdir -p /mnt/rootfs/home/tinexus/Desktop /mnt/rootfs/etc/skel/Desktop
if [ -f "/mnt/rootfs/usr/share/applications/tinexus-installer.desktop" ]; then
    cp -f /mnt/rootfs/usr/share/applications/tinexus-installer.desktop /mnt/rootfs/home/tinexus/Desktop/
    cp -f /mnt/rootfs/usr/share/applications/tinexus-installer.desktop /mnt/rootfs/etc/skel/Desktop/
fi
chmod 0755 /mnt/rootfs/home/tinexus/Desktop/*.desktop /mnt/rootfs/etc/skel/Desktop/*.desktop 2>/dev/null || true
if [ -d /mnt/rootfs/home/tinexus ]; then
    chroot /mnt/rootfs chown -R 1000:1000 /home/tinexus/Desktop 2>/dev/null || true
fi

# ── 10. Stage Wallpapers, Logos and Libraries ──────────────────────────────────
info "Staging wallpapers and logos into rootfs..."
mkdir -p /mnt/rootfs/usr/share/backgrounds /mnt/rootfs/usr/share/tinexus /mnt/rootfs/usr/share/pixmaps /mnt/rootfs/usr/share/icons/hicolor/32x32/apps
cp -a /workspace/assets/wallpaper/* /mnt/rootfs/usr/share/backgrounds/ 2>/dev/null || true
chmod 0644 /mnt/rootfs/usr/share/backgrounds/* 2>/dev/null || true
ln -sf /usr/share/backgrounds/emerald-matrix.png /mnt/rootfs/usr/share/backgrounds/default.png 2>/dev/null || true

# Pre-populate settings template with Emerald Matrix default
mkdir -p /mnt/rootfs/etc/tinexus /mnt/rootfs/etc/skel/.config/tinexus
cat << 'EOF' > /mnt/rootfs/etc/tinexus/settings.toml
# Tinexus Desktop Settings Configuration
accent_index = 0
selected_wallpaper_idx = 0
wallpaper_path = "/usr/share/backgrounds/emerald-matrix.png"
theme_mode = "Dark"
display_scale_idx = 0
night_light = false
vrr_enabled = false
screen_timeout_min = 15
sleep_after_min = 30
power_profile_idx = 0
lock_on_sleep = true
pam_auth = true
clipboard_history_size = 50

[wallpaper]
path = "/usr/share/backgrounds/emerald-matrix.png"
mode = "fill"
EOF
cp -f /mnt/rootfs/etc/tinexus/settings.toml /mnt/rootfs/etc/skel/.config/tinexus/settings.toml 2>/dev/null || true

cp -f /workspace/assets/logo/tinexus-logo.png /mnt/rootfs/tinexus-logo.png
cp -f /workspace/assets/logo/tinexus-logo.png /mnt/rootfs/usr/share/tinexus/tinexus-logo.png
cp -f /workspace/assets/logo/tinexus-logo.png /mnt/rootfs/usr/share/pixmaps/tinexus-logo.png
cp -f /workspace/assets/logo/tinexus-logo.png /mnt/rootfs/usr/share/icons/hicolor/32x32/apps/tinexus-logo.png

mkdir -p /mnt/rootfs/usr/share/sounds/tinexus
cp -a /workspace/assets/sounds/* /mnt/rootfs/usr/share/sounds/tinexus/ 2>/dev/null || true
chmod 0644 /mnt/rootfs/usr/share/sounds/tinexus/* 2>/dev/null || true

info "Running ldconfig in rootfs..."
chroot /mnt/rootfs /sbin/ldconfig

# ── 11. File Capabilities on Network Binaries (NO sudo) ───────────────────────
info "Applying file capabilities to network binaries via setcap inside rootfs..."
for raw_bin in /usr/sbin/dhclient /sbin/dhclient; do
    if [ -f "/mnt/rootfs$raw_bin" ]; then
        real_bin="$(chroot /mnt/rootfs realpath "$raw_bin" 2>/dev/null || echo "$raw_bin")"
        chroot /mnt/rootfs setcap cap_net_admin,cap_net_raw,cap_net_bind_service+ep "$real_bin" || true
        info "setcap applied to $real_bin"
        chroot /mnt/rootfs getcap "$real_bin" || true
    fi
done
for raw_bin in /usr/sbin/wpa_supplicant /sbin/wpa_supplicant; do
    if [ -f "/mnt/rootfs$raw_bin" ]; then
        real_bin="$(chroot /mnt/rootfs realpath "$raw_bin" 2>/dev/null || echo "$raw_bin")"
        chroot /mnt/rootfs setcap cap_net_admin,cap_net_raw+ep "$real_bin" || true
        info "setcap applied to $real_bin"
        chroot /mnt/rootfs getcap "$real_bin" || true
    fi
done
for raw_bin in /bin/busybox /usr/bin/udhcpc /sbin/udhcpc; do
    if [ -f "/mnt/rootfs$raw_bin" ] && [ ! -L "/mnt/rootfs$raw_bin" ]; then
        real_bin="$(chroot /mnt/rootfs realpath "$raw_bin" 2>/dev/null || echo "$raw_bin")"
        chroot /mnt/rootfs setcap cap_net_admin,cap_net_raw,cap_net_bind_service+ep "$real_bin" || true
        info "setcap applied to $real_bin"
        chroot /mnt/rootfs getcap "$real_bin" || true
    fi
done
for raw_bin in /usr/bin/ip /bin/ip /sbin/ip /usr/sbin/ip /usr/sbin/iw /usr/bin/iw; do
    if [ -f "/mnt/rootfs$raw_bin" ]; then
        real_bin="$(chroot /mnt/rootfs realpath "$raw_bin" 2>/dev/null || echo "$raw_bin")"
        chroot /mnt/rootfs setcap cap_net_admin,cap_net_raw+ep "$real_bin" || true
        info "setcap applied to $real_bin"
    fi
done
mkdir -p /mnt/rootfs/var/lib/dhcp
chmod 1777 /mnt/rootfs/var/lib/dhcp

mkdir -p /mnt/rootfs/var/run/wpa_supplicant /mnt/rootfs/run/wpa_supplicant
chown -R root:netdev /mnt/rootfs/var/run/wpa_supplicant /mnt/rootfs/run/wpa_supplicant 2>/dev/null || true
chmod 1777 /mnt/rootfs/var/run/wpa_supplicant /mnt/rootfs/run/wpa_supplicant

mkdir -p /mnt/rootfs/etc/tmpfiles.d
cat << 'EOF' > /mnt/rootfs/etc/tmpfiles.d/wpa_supplicant.conf
d /run/wpa_supplicant 1777 root netdev - -
d /var/run/wpa_supplicant 1777 root netdev - -
EOF

mkdir -p /mnt/rootfs/etc/wpa_supplicant
cat << 'EOF' > /mnt/rootfs/etc/wpa_supplicant/wpa_supplicant.conf
ctrl_interface=/var/run/wpa_supplicant
ctrl_interface_group=netdev
update_config=1
EOF
chmod 0644 /mnt/rootfs/etc/wpa_supplicant/wpa_supplicant.conf
success "Network binaries and socket paths are capability-hardened (no sudo needed for tinexus user)."

# ── 12. Systematic Automated Rootfs Verification Pass ─────────────────────────
info "Running automated assertion: Verifying EVERY file promised by ANY .deb package exists in rootfs..."
missing_files=0
for deb in /workspace/build/debs/*.deb; do
    [ -f "$deb" ] || continue
    pkg_name="$(chroot /mnt/rootfs /usr/bin/dpkg-deb -f "$deb" Package 2>/dev/null || basename "$deb")"
    info "Auditing packaged payload of '$pkg_name' ($(basename "$deb"))..."
    
    while IFS= read -r line; do
        [[ "$line" =~ ^d ]] && continue
        raw_target="$(echo "$line" | awk '{print $NF}')"
        if [[ "$line" =~ \ -\>\  ]]; then
            raw_target="$(echo "$line" | sed -n 's/.* \(\.\/[^ ]*\) -> .*/\1/p')"
        fi
        clean_target="${raw_target#./}"
        [ -n "$clean_target" ] || continue
        
        if [ ! -e "/mnt/rootfs/$clean_target" ] && [ ! -L "/mnt/rootfs/$clean_target" ]; then
            echo -e "\e[1;31m[MISSING FILE ERROR]\e[0m Package '$pkg_name' promises '/$clean_target', but it is MISSING from rootfs!" >&2
            missing_files=$((missing_files + 1))
        fi
    done < <(chroot /mnt/rootfs /usr/bin/dpkg-deb -c "$deb")
done

if [ "$missing_files" -gt 0 ]; then
    fatal "Automated rootfs assertion FAILED: $missing_files packaged files are missing from rootfs!"
fi
success "Automated rootfs assertion PASSED: 100% of files across all .deb packages are confirmed present in rootfs."

# ── 13. Clean Up Rootfs Temporary Files ───────────────────────────────────────
info "Cleaning apt caches in rootfs..."
chroot /mnt/rootfs /usr/bin/apt-get clean
rm -rf /mnt/rootfs/var/lib/apt/lists/* /mnt/rootfs/tmp/* /mnt/rootfs/var/tmp/*

# ── 14. Unmount Chroot Bindings & Rigorous Leak Verification ──────────────────
info "Terminating lingering chroot processes..."
fuser -km /mnt/rootfs 2>/dev/null || true
sleep 1

info "Unmounting all chroot bindings..."
# Innermost submounts first
for m in /mnt/rootfs/dev/pts /mnt/rootfs/dev /mnt/rootfs/workspace /mnt/rootfs/proc /mnt/rootfs/sys /mnt/rootfs/run /mnt/rootfs/tmp; do
    while mount | grep " $m " >/dev/null 2>&1; do
        info "Unmounting $m..."
        umount "$m" 2>/dev/null || umount -l "$m" 2>/dev/null || break
        sleep 0.2
    done
done

info "Verifying zero leaked runtime mounts under /mnt/rootfs..."
leaked_mounts="$(mount | grep '/mnt/rootfs/' || true)"
if [ -n "$leaked_mounts" ]; then
    fatal "CRITICAL BUILD ERROR: Leaked runtime mounts detected under /mnt/rootfs:\n$leaked_mounts\nAborting before mksquashfs to prevent ISO contamination!"
fi
success "Verified: Zero leaked mounts under /mnt/rootfs."

# Clean up temporary directories and mount point stubs
info "Cleaning and sanitizing rootfs mount points..."
rm -rf /mnt/rootfs/tmp/* /mnt/rootfs/var/tmp/* /mnt/rootfs/root/.bash_history
find /mnt/rootfs/workspace -mindepth 1 -delete 2>/dev/null || true
find /mnt/rootfs/proc -mindepth 1 -delete 2>/dev/null || true
find /mnt/rootfs/sys -mindepth 1 -delete 2>/dev/null || true

# Assert mount directories are completely empty
[ -z "$(ls -A /mnt/rootfs/workspace 2>/dev/null)" ] || fatal "CRITICAL: /mnt/rootfs/workspace is not empty after unmount!"
[ -z "$(ls -A /mnt/rootfs/proc 2>/dev/null)" ] || fatal "CRITICAL: /mnt/rootfs/proc is not empty after unmount!"
[ -z "$(ls -A /mnt/rootfs/sys 2>/dev/null)" ] || fatal "CRITICAL: /mnt/rootfs/sys is not empty after unmount!"

# Check uncompressed rootfs size
rootfs_kb="$(du -s /mnt/rootfs | awk '{print $1}')"
info "Uncompressed rootfs size: $((rootfs_kb / 1024)) MB"
if [ "$rootfs_kb" -gt 15000000 ]; then
    fatal "CRITICAL BUILD ERROR: Uncompressed rootfs size ($((rootfs_kb / 1024)) MB) exceeds 15000 MB! Contamination detected."
fi

# ── 15. Create New SquashFS ───────────────────────────────────────────────────
info "Compressing clean rootfs with mksquashfs (xz)..."
rm -f "$NEW_SQUASHFS"
mksquashfs /mnt/rootfs "$NEW_SQUASHFS" \
    -comp xz \
    -b 1048576 \
    -Xbcj x86 \
    -noappend \
    -wildcards \
    -e 'workspace/*' 'tmp/*' 'var/tmp/*'
success "Created new squashfs: $(ls -lh "$NEW_SQUASHFS" | awk '{print $5}')"

# ── 15b. Rigorous SquashFS Content Verification ───────────────────────────────
info "Auditing generated SquashFS contents for runtime leaks..."
squashfs_mb="$(du -m "$NEW_SQUASHFS" | awk '{print $1}')"
info "Generated SquashFS size: ${squashfs_mb} MB (Expected: 650 MB – 3500 MB)"
if [ "$squashfs_mb" -gt 3500 ] || [ "$squashfs_mb" -lt 650 ]; then
    fatal "CRITICAL BUILD ERROR: Abnormal SquashFS size (${squashfs_mb} MB)! Expected between 650MB and 3500MB."
fi

leaked_files="$(unsquashfs -l "$NEW_SQUASHFS" | grep -E '^squashfs-root/(proc/|sys/|workspace/[a-zA-Z0-9])' || true)"
if [ -n "$leaked_files" ]; then
    fatal "CRITICAL BUILD ERROR: Leaked runtime files found inside rootfs.squashfs:\n$leaked_files"
fi
success "SquashFS content verification PASSED: Zero leaked runtime files detected."

# ── 16. Rebuild Initramfs & Replay ISO Boot Configuration ───────────────────
info "Rebuilding initramfs with /sbin/init systemd target..."
bash /workspace/tools/rebuild_initramfs.sh
NEW_INITRAMFS="/workspace/build/kernel/initramfs.img"
[ -f "$NEW_INITRAMFS" ] || fatal "Updated initramfs not found at $NEW_INITRAMFS"
info "Verified updated initramfs: $(ls -lh "$NEW_INITRAMFS" | awk '{print $5}')"

info "Assembling updated ISO via xorriso replay..."
rm -f "$NEW_ISO"
xorriso -indev "$ORIG_ISO" \
        -outdev "$NEW_ISO" \
        -update "$NEW_SQUASHFS" /live/rootfs.squashfs \
        -update "$NEW_INITRAMFS" /boot/initramfs.img \
        -boot_image any replay

success "Updated ISO created at $NEW_ISO"

# ── 17. Verify Boot Catalog ───────────────────────────────────────────────────
info "Verifying updated ISO boot catalog..."
xorriso -indev "$NEW_ISO" -report_el_torito plain

# ── 18. Atomic Replacement & SHA256 ───────────────────────────────────────────
info "Replacing original ISO with updated ISO..."
cp -f "$ORIG_ISO" "$BUILD_DIR/Tinexus-x86_64.iso.bak"
mv -f "$NEW_ISO" "$ORIG_ISO"
rm -f "$NEW_SQUASHFS"

info "Generating SHA256 checksum..."
sha256sum "$ORIG_ISO" > "$ORIG_ISO.sha256"

success "========================================================"
success "Successfully rebuilt Tinexus-x86_64.iso with all fixes!"
success "ISO Path: $ORIG_ISO"
success "ISO Size: $(ls -lh "$ORIG_ISO" | awk '{print $5}')"
success "SHA256:   $(cat "$ORIG_ISO.sha256")"
success "========================================================"
