#!/bin/bash
# ==============================================================================
# Tinexus Platform — Rebuild Initramfs with /sbin/init (systemd) Target
# ==============================================================================
set -euo pipefail

PROJECT_DIR="/workspace"
BUILD_DIR="$PROJECT_DIR/build"
ORIG_ISO="$BUILD_DIR/Tinexus-x86_64.iso"
WORK_DIR="/mnt/workdisk/initramfs_rebuild"

[ -f "$BUILD_DIR/workdisk.img" ] || { echo "workdisk.img not found!"; exit 1; }
mkdir -p /mnt/workdisk
mountpoint -q /mnt/workdisk || mount -o loop "$BUILD_DIR/workdisk.img" /mnt/workdisk

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR/extracted" "$WORK_DIR/isomnt_init"

echo "[INFO] Mounting original ISO to obtain base initramfs..."
mount -o loop,ro "$ORIG_ISO" "$WORK_DIR/isomnt_init"
cp -f "$WORK_DIR/isomnt_init/boot/initramfs.img" "$WORK_DIR/initramfs_orig.img"
umount "$WORK_DIR/isomnt_init"

echo "[INFO] Unpacking initramfs..."
cd "$WORK_DIR/extracted"
gzip -dc "$WORK_DIR/initramfs_orig.img" | cpio -idmv 2>/dev/null

echo "[INFO] Verifying original /init script:"
grep -n "switch_root" init

echo "[INFO] Patching /init script using sed to target /sbin/init (systemd)..."
sed -i 's|log_step "STEP 11: ABOUT TO EXECUTE switch_root -c /dev/console /newroot /usr/bin/tinexus-serviced..."|log_step "STEP 11: ABOUT TO EXECUTE switch_root into systemd init (/sbin/init)..."|g' init
sed -i 's|exec switch_root -c /dev/console /newroot /usr/bin/tinexus-serviced|exec switch_root -c /dev/console /newroot /sbin/init|g' init
# Eliminate triplicated logging lines from log_step
sed -i '/echo ">>> \[TINEXUS-STEP\] \$1" > \/dev\/console/d' init
sed -i '/echo ">>> \[TINEXUS-STEP\] \$1" > \/dev\/tty0/d' init

chmod 0755 init
cp -f "$PROJECT_DIR/assets/logo/tinexus-logo.png" "$WORK_DIR/extracted/tinexus-logo.png"

echo "[INFO] Verifying patched /init script:"
grep -n -C 4 "switch_root" init

# Assert tinexus-serviced is NO LONGER target of switch_root
if grep "switch_root" init | grep "tinexus-serviced" >/dev/null 2>&1; then
    echo "[FATAL] tinexus-serviced is still present in switch_root call!"
    exit 1
fi

echo "[INFO] Repacking updated initramfs..."
mkdir -p "$BUILD_DIR/kernel"
OUTPUT_INITRAMFS="$BUILD_DIR/kernel/initramfs.img"
rm -f "$OUTPUT_INITRAMFS"
find . | cpio -o -H newc 2>/dev/null | gzip -9 > "$OUTPUT_INITRAMFS"

echo "[SUCCESS] New initramfs generated at $OUTPUT_INITRAMFS:"
ls -lh "$OUTPUT_INITRAMFS"

# Verify the new initramfs
echo "[INFO] Verifying newly generated initramfs content..."
mkdir -p "$WORK_DIR/verify"
(cd "$WORK_DIR/verify" && gzip -dc "$OUTPUT_INITRAMFS" | cpio -idmv init 2>/dev/null)
grep -n -C 3 "switch_root" "$WORK_DIR/verify/init"

cd /workspace
rm -rf "$WORK_DIR"
echo "[SUCCESS] Initramfs rebuild & verification complete."
