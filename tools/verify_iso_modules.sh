#!/usr/bin/env bash
set -euo pipefail

ISO="/mnt/e/Tinu's Technology/Tinexus Manager/build/Tinexus-x86_64.iso"
MNT=$(mktemp -d)
trap 'umount "$MNT" 2>/dev/null || true; rm -rf "$MNT"' EXIT

mount -o loop,ro "$ISO" "$MNT"

echo "=== Initramfs Wi-Fi Modules ==="
zcat "$MNT/boot/initramfs.img" | cpio -t 2>/dev/null | grep -E 'libarc4|mac80211|cfg80211|mt76|mt792'

echo "=== Initramfs MediaTek Firmware ==="
zcat "$MNT/boot/initramfs.img" | cpio -t 2>/dev/null | grep 'lib/firmware/mediatek' | head -n 10

echo "=== Rootfs Wi-Fi Modules ==="
unsquashfs -l "$MNT/live/rootfs.squashfs" 2>/dev/null | grep -E 'libarc4|mac80211|cfg80211|mt76|mt792'

echo "=== Verification SUCCESS ==="
