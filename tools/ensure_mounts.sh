#!/bin/bash
set -euo pipefail

hwclock -s 2>/dev/null || true

ORIG_ISO="/workspace/build/Tinexus-x86_64.iso"

if [ -f "/workspace/build/workdisk.img" ]; then
    mkdir -p /mnt/workdisk
    mountpoint -q /mnt/workdisk || mount -o loop "/workspace/build/workdisk.img" /mnt/workdisk 2>/dev/null || true
fi

if mountpoint -q /mnt/workdisk; then
    UPPER_DIR="/mnt/workdisk/upper"
    WORK_DIR="/mnt/workdisk/work"
else
    UPPER_DIR="/var/tmp/upper"
    WORK_DIR="/var/tmp/work"
fi

mkdir -p /mnt/isomnt /mnt/squashfs /mnt/rootfs "$UPPER_DIR" "$WORK_DIR"

if ! mountpoint -q /mnt/isomnt; then
    mount -o loop,ro "$ORIG_ISO" /mnt/isomnt 2>/dev/null || true
fi

if ! mountpoint -q /mnt/squashfs; then
    mount -o loop,ro /mnt/isomnt/live/rootfs.squashfs /mnt/squashfs 2>/dev/null || true
fi

if ! mountpoint -q /mnt/rootfs; then
    mount -t overlay overlay -o lowerdir=/mnt/squashfs,upperdir="$UPPER_DIR",workdir="$WORK_DIR" /mnt/rootfs 2>/dev/null || true
fi

mountpoint -q /mnt/rootfs/proc || mount --bind /proc /mnt/rootfs/proc 2>/dev/null || true
mountpoint -q /mnt/rootfs/sys || mount --bind /sys /mnt/rootfs/sys 2>/dev/null || true
mountpoint -q /mnt/rootfs/dev || mount --bind /dev /mnt/rootfs/dev 2>/dev/null || true
mkdir -p /mnt/rootfs/dev/pts
mountpoint -q /mnt/rootfs/dev/pts || mount -t devpts devpts /mnt/rootfs/dev/pts -o ptmxmode=0666,mode=620,newinstance 2>/dev/null || mount --bind /dev/pts /mnt/rootfs/dev/pts 2>/dev/null || true
chmod 666 /mnt/rootfs/dev/pts/ptmx 2>/dev/null || true
chmod 666 /mnt/rootfs/dev/ptmx 2>/dev/null || true
mkdir -p /mnt/rootfs/workspace
mountpoint -q /mnt/rootfs/workspace || mount --bind /workspace /mnt/rootfs/workspace 2>/dev/null || true

# Ensure rootfs /tmp uses workdisk if available to avoid /dev/shm or rootfs exhaustion
if mountpoint -q /mnt/workdisk; then
    mkdir -p /mnt/workdisk/tmp
    chmod 1777 /mnt/workdisk/tmp
    mkdir -p /mnt/rootfs/tmp
    mountpoint -q /mnt/rootfs/tmp || mount --bind /mnt/workdisk/tmp /mnt/rootfs/tmp 2>/dev/null || true
fi
chmod 1777 /mnt/rootfs/tmp 2>/dev/null || true
