#!/bin/bash
# ==============================================================================
# Tinexus Platform — Fast Incremental ISO Updater
# Injects compiled binaries & updated QML assets into live squashfs without full recompile
# ==============================================================================
set -euo pipefail

hwclock -s 2>/dev/null || true

PROJECT_DIR="/workspace"
BUILD_DIR="$PROJECT_DIR/build"
ORIG_ISO="$BUILD_DIR/Tinexus-x86_64.iso"
NEW_ISO="$BUILD_DIR/Tinexus-x86_64-updated.iso"

echo "[INFO] Ensuring mounts..."
bash /workspace/tools/ensure_mounts.sh

if mountpoint -q /mnt/workdisk; then
    NEW_SQUASHFS="/mnt/workdisk/rootfs.squashfs"
else
    NEW_SQUASHFS="/dev/shm/rootfs.squashfs"
fi

echo "[INFO] Installing updated binaries into rootfs..."
CORE_BINARIES=(
    "tinexus-settings"
    "tinexus-settings-ui"
    "tinexus-session"
    "tinexus-notifications"
    "tinexus-shell"
    "tinexus-comp"
    "tinexus-dock"
    "tinexus-launcher"
    "tinexus-wallpaper"
    "tinexus-lock"
    "tinexus-terminal"
    "tinexus-serviced"
    "tinexus-ipcd"
    "tinexus-files"
    "tinexus-monitor"
    "tinexus-about"
    "tinexus-store"
)

for b in "${CORE_BINARIES[@]}"; do
    if [ -f "/workspace/build/bin/$b" ]; then
        echo "[INFO] Syncing $b -> /mnt/rootfs/usr/bin/$b"
        rm -f "/mnt/rootfs/usr/bin/$b"
        cp -f "/workspace/build/bin/$b" "/mnt/rootfs/usr/bin/$b"
        chmod 755 "/mnt/rootfs/usr/bin/$b"
        chroot /mnt/rootfs /usr/bin/strip "/usr/bin/$b" 2>/dev/null || true
    fi
done

echo "[INFO] Syncing updated QML files into rootfs..."
QML_MODULES=("shell" "settings-ui" "dock" "launcher" "lock" "wallpaper" "files" "monitor" "terminal" "common")
for m in "${QML_MODULES[@]}"; do
    if [ -d "/workspace/src/$m/qml" ]; then
        mkdir -p "/mnt/rootfs/usr/share/tinexus/$m/qml"
        cp -rf /workspace/src/$m/qml/* "/mnt/rootfs/usr/share/tinexus/$m/qml/"
        # Sync legacy compatibility symlink/path if present
        mkdir -p "/mnt/rootfs/usr/share/tinexus-$m/qml" 2>/dev/null || true
        cp -rf /workspace/src/$m/qml/* "/mnt/rootfs/usr/share/tinexus-$m/qml/" 2>/dev/null || true
    fi
done

# Ensure tinexus-settings compatibility path
mkdir -p /mnt/rootfs/usr/share/tinexus-settings/qml 2>/dev/null || true
cp -rf /workspace/src/settings-ui/qml/* /mnt/rootfs/usr/share/tinexus-settings/qml/ 2>/dev/null || true

echo "[INFO] Verifying rootfs binaries..."
ls -lh /mnt/rootfs/usr/bin/tinexus-settings \
       /mnt/rootfs/usr/bin/tinexus-settings-ui \
       /mnt/rootfs/usr/bin/tinexus-session \
       /mnt/rootfs/usr/bin/tinexus-notifications \
       /mnt/rootfs/usr/bin/tinexus-shell

echo "[INFO] Unmounting chroot bind mounts before squashfs..."
umount -l /mnt/rootfs/workspace 2>/dev/null || true
umount -l /mnt/rootfs/dev/pts 2>/dev/null || true
umount -l /mnt/rootfs/dev 2>/dev/null || true
umount -l /mnt/rootfs/proc 2>/dev/null || true
umount -l /mnt/rootfs/sys 2>/dev/null || true
umount -l /mnt/rootfs/tmp 2>/dev/null || true

rm -rf /mnt/rootfs/tmp/* /mnt/rootfs/var/tmp/* /mnt/rootfs/root/.bash_history 2>/dev/null || true

echo "[INFO] Compressing updated rootfs into squashfs..."
rm -f "$NEW_SQUASHFS"
mksquashfs /mnt/rootfs "$NEW_SQUASHFS" \
    -comp xz \
    -b 1048576 \
    -Xbcj x86 \
    -noappend \
    -wildcards \
    -e 'workspace/*' 'tmp/*' 'var/tmp/*'

echo "[INFO] Updating ISO with new squashfs via xorriso replay..."
rm -f "$NEW_ISO"
xorriso -indev "$ORIG_ISO" \
        -outdev "$NEW_ISO" \
        -update "$NEW_SQUASHFS" /live/rootfs.squashfs \
        -boot_image any replay

echo "[INFO] Verifying updated ISO boot catalog..."
xorriso -indev "$NEW_ISO" -report_el_torito plain

echo "[INFO] Replacing original ISO with updated ISO..."
cp -f "$ORIG_ISO" "$BUILD_DIR/Tinexus-x86_64.iso.bak" 2>/dev/null || true
mv -f "$NEW_ISO" "$ORIG_ISO"
rm -f "$NEW_SQUASHFS"

echo "[INFO] Generating SHA256 checksum..."
sha256sum "$ORIG_ISO" > "$ORIG_ISO.sha256"

echo ""
echo "[SUCCESS] Successfully rebuilt Tinexus-x86_64.iso with all fixes!"
echo "[SUCCESS] ISO Path: $ORIG_ISO"
echo "[SUCCESS] ISO Size: $(ls -lh "$ORIG_ISO" | awk '{print $5}')"
echo "[SUCCESS] SHA256:   $(cat "$ORIG_ISO.sha256")"
