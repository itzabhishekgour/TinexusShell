#!/bin/bash
set -euo pipefail

ORIG_ISO="/workspace/build/Tinexus-x86_64.iso"
NEW_ISO="/workspace/build/Tinexus-x86_64-updated.iso"

if [ -f "/workspace/build/workdisk.img" ]; then
    mkdir -p /mnt/workdisk
    mountpoint -q /mnt/workdisk || mount -o loop "/workspace/build/workdisk.img" /mnt/workdisk 2>/dev/null || true
fi

NEW_SQUASHFS="/mnt/workdisk/rootfs.squashfs"

echo "[INFO] Ensuring mounts..."
bash /workspace/tools/ensure_mounts.sh

echo "[INFO] Installing updated binaries into rootfs..."
# 1. tinexus-settings (Pure C++20 daemon - ensure it's not a symlink)
rm -f /mnt/rootfs/usr/bin/tinexus-settings
cp -f /workspace/build/bin/tinexus-settings /mnt/rootfs/usr/bin/tinexus-settings
chmod 755 /mnt/rootfs/usr/bin/tinexus-settings
chroot /mnt/rootfs /usr/bin/strip /usr/bin/tinexus-settings 2>/dev/null || true

# 2. tinexus-settings-ui (Qt6 / QML settings application)
cp -f /workspace/build/bin/tinexus-settings-ui /mnt/rootfs/usr/bin/tinexus-settings-ui
chmod 755 /mnt/rootfs/usr/bin/tinexus-settings-ui
chroot /mnt/rootfs /usr/bin/strip /usr/bin/tinexus-settings-ui 2>/dev/null || true

# 3. tinexus-session (Platform session supervisor)
cp -f /workspace/build/bin/tinexus-session /mnt/rootfs/usr/bin/tinexus-session
chmod 755 /mnt/rootfs/usr/bin/tinexus-session
chroot /mnt/rootfs /usr/bin/strip /usr/bin/tinexus-session 2>/dev/null || true

# 4. tinexus-notifications (Pure C++20 sd-bus notification daemon)
cp -f /workspace/build/bin/tinexus-notifications /mnt/rootfs/usr/bin/tinexus-notifications
chmod 755 /mnt/rootfs/usr/bin/tinexus-notifications
chroot /mnt/rootfs /usr/bin/strip /usr/bin/tinexus-notifications 2>/dev/null || true

# 5. tinexus-shell (Qt6 / QML desktop shell with NotificationToast)
cp -f /workspace/build/bin/tinexus-shell /mnt/rootfs/usr/bin/tinexus-shell
chmod 755 /mnt/rootfs/usr/bin/tinexus-shell
chroot /mnt/rootfs /usr/bin/strip /usr/bin/tinexus-shell 2>/dev/null || true

echo "[INFO] Syncing updated QML files into rootfs..."
mkdir -p /mnt/rootfs/usr/share/tinexus/shell/qml
mkdir -p /mnt/rootfs/usr/share/tinexus-shell/qml
cp -rf /workspace/src/shell/qml/* /mnt/rootfs/usr/share/tinexus/shell/qml/
cp -rf /workspace/src/shell/qml/* /mnt/rootfs/usr/share/tinexus-shell/qml/

mkdir -p /mnt/rootfs/usr/share/tinexus/settings-ui/qml
mkdir -p /mnt/rootfs/usr/share/tinexus-settings/qml
mkdir -p /mnt/rootfs/usr/share/tinexus-settings-ui/qml
cp -rf /workspace/src/settings-ui/qml/* /mnt/rootfs/usr/share/tinexus/settings-ui/qml/
cp -rf /workspace/src/settings-ui/qml/* /mnt/rootfs/usr/share/tinexus-settings/qml/
cp -rf /workspace/src/settings-ui/qml/* /mnt/rootfs/usr/share/tinexus-settings-ui/qml/

mkdir -p /mnt/rootfs/usr/share/tinexus/common/qml
cp -rf /workspace/src/common/qml/* /mnt/rootfs/usr/share/tinexus/common/qml/

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

echo "[INFO] Updating ISO with new squashfs via xorriso..."
rm -f "$NEW_ISO"
xorriso -indev "$ORIG_ISO" \
        -outdev "$NEW_ISO" \
        -update "$NEW_SQUASHFS" /live/rootfs.squashfs \
        -boot_image any replay

echo "[INFO] Replacing original ISO with updated ISO..."
mv -f "$NEW_ISO" "$ORIG_ISO"
rm -f "$NEW_SQUASHFS"

echo "[SUCCESS] ISO successfully updated with all latest binaries and QML UI components!"
