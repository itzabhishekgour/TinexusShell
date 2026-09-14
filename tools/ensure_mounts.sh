#!/bin/bash
set -e
ORIG_ISO="/workspace/build/Tinexus-x86_64.iso"
mkdir -p /mnt/isomnt /mnt/squashfs /mnt/rootfs /dev/shm/upper /dev/shm/work

if ! mount | grep -q '/mnt/isomnt'; then
    mount -o loop,ro "$ORIG_ISO" /mnt/isomnt
fi

if ! mount | grep -q '/mnt/squashfs'; then
    mount -o loop,ro /mnt/isomnt/live/rootfs.squashfs /mnt/squashfs
fi

if ! mount | grep -q '/mnt/rootfs'; then
    mount -t overlay overlay -o lowerdir=/mnt/squashfs,upperdir=/dev/shm/upper,workdir=/dev/shm/work /mnt/rootfs
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
