#!/bin/sh
set -e

ln -sfn "/mnt/host/e/Tinu's Technology/Tinexus Manager" /workspace

echo "=== CREATING & MOUNTING 8GB WORKDISK ==="
mkdir -p /workspace/build
if [ ! -f /workspace/build/workdisk.img ]; then
    truncate -s 8G /workspace/build/workdisk.img
    mkfs.ext4 -F /workspace/build/workdisk.img
fi

mkdir -p /mnt/workdisk
if ! mountpoint -q /mnt/workdisk; then
    mount -o loop /workspace/build/workdisk.img /mnt/workdisk
fi
df -h /mnt/workdisk

echo "=== MOUNTING ISO ==="
mkdir -p /mnt/isomnt
mount -o loop,ro /workspace/build/Tinexus-x86_64.iso /mnt/isomnt

echo "=== EXTRACTING ROOTFS.SQUASHFS TO /mnt/workdisk/rootfs ==="
rm -rf /mnt/workdisk/rootfs
unsquashfs -d /mnt/workdisk/rootfs /mnt/isomnt/live/rootfs.squashfs

echo "=== UNMOUNTING ISO ==="
umount /mnt/isomnt

echo "=== ROOTFS READY ON WORKDISK ==="
df -h /mnt/workdisk
ls -la /mnt/workdisk/rootfs
