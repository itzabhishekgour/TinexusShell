#!/usr/bin/env bash
set -euo pipefail

WORK=$(mktemp -d /tmp/iso_verify_XXXXXX)
cleanup() {
    rm -rf "$WORK"
}
trap cleanup EXIT

ISO="/home/mrasg/tinexus/build/Tinexus-x86_64.iso"
if [ ! -f "$ISO" ]; then
    ISO="/mnt/e/Tinu's Technology/Tinexus Manager/build/Tinexus-x86_64.iso"
fi

echo "=== 1. Extracting squashfs and initramfs from ISO ==="
xorriso -osirrox on -indev "$ISO" -extract /live/rootfs.squashfs "$WORK/rootfs.squashfs"
xorriso -osirrox on -indev "$ISO" -extract /boot/initramfs.img "$WORK/initramfs.img"

echo "=== 2. Extracting tinexus-shell, tinexus-serviced, tinexus-settings-ui, and sof-priority.conf from rootfs.squashfs ==="
unsquashfs -d "$WORK/squash_extracted" "$WORK/rootfs.squashfs" "usr/bin/tinexus-shell" "usr/bin/tinexus-serviced" "usr/bin/tinexus-settings-ui" "etc/modprobe.d/sof-priority.conf"

SHELL_BIN="$WORK/squash_extracted/usr/bin/tinexus-shell"
SERVICED_BIN="$WORK/squash_extracted/usr/bin/tinexus-serviced"
SETTINGS_BIN="$WORK/squash_extracted/usr/bin/tinexus-settings-ui"
SOF_CONF="$WORK/squash_extracted/etc/modprobe.d/sof-priority.conf"

echo "=== 3. SHA256 Checksums inside ISO rootfs ==="
sha256sum "$SHELL_BIN"
sha256sum "$SERVICED_BIN"
sha256sum "$SETTINGS_BIN"

echo "=== 4. SHA256 Checksums in ~/tinexus/build/debug/bin/ ==="
sha256sum /home/mrasg/tinexus/build/debug/bin/tinexus-shell
sha256sum /home/mrasg/tinexus/build/debug/bin/tinexus-serviced
sha256sum /home/mrasg/tinexus/build/debug/bin/tinexus-settings-ui

echo "=== 5. Verifying /etc/modprobe.d/sof-priority.conf in rootfs ==="
cat "$SOF_CONF"

echo "=== 6. Verifying /etc/modprobe.d/sof-priority.conf in initramfs ==="
mkdir -p "$WORK/initramfs_extracted"
(cd "$WORK/initramfs_extracted" && gzip -dc "$WORK/initramfs.img" | cpio -idmv "etc/modprobe.d/*" 2>/dev/null) || true
cat "$WORK/initramfs_extracted/etc/modprobe.d/sof-priority.conf"

echo "=== 7. String / Symbol confirmation in ISO tinexus-shell ==="
strings "$SHELL_BIN" | grep -E "remove_notification|on_notification_click" || true
strings "$SHELL_BIN" | grep -E "compute_input_region|TOTAL_BAR_HEIGHT|BAR_HEIGHT" || true

echo "=== 8. String / Symbol confirmation in ISO tinexus-settings-ui ==="
strings "$SETTINGS_BIN" | grep -E "No output devices detected|Universal Audio Controller" || true

echo "=== 9. ELF Build-IDs ==="
readelf -n "$SHELL_BIN" | grep "Build ID" || true
readelf -n /home/mrasg/tinexus/build/debug/bin/tinexus-shell | grep "Build ID" || true

echo "=== ALL CHECKS COMPLETED SUCCESSFULLY! ==="
