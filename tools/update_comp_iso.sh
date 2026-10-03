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

echo "[INFO] Copying new tinexus-comp binary to rootfs..."
cp -f /workspace/build/bin/tinexus-comp /mnt/rootfs/usr/bin/tinexus-comp
chmod 755 /mnt/rootfs/usr/bin/tinexus-comp
chroot /mnt/rootfs /usr/bin/strip /usr/bin/tinexus-comp 2>/dev/null || true

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

echo "[SUCCESS] ISO successfully updated with latest tinexus-comp!"
