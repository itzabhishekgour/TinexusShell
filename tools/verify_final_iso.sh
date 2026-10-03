#!/bin/bash
set -euo pipefail

echo "========================================================"
echo "         TINEXUS FINAL ISO & BINARY AUDIT GATE"
echo "========================================================"

ISO="/workspace/build/Tinexus-x86_64.iso"
WORKDISK="/workspace/build/workdisk.img"
mkdir -p /mnt/workdisk
mountpoint -q /mnt/workdisk || mount -o loop "$WORKDISK" /mnt/workdisk

AUDIT_DIR="/mnt/workdisk/verify_audit"
rm -rf "$AUDIT_DIR"
mkdir -p "$AUDIT_DIR"

MNT_ISO="$AUDIT_DIR/iso_mnt"
MNT_SQ="$AUDIT_DIR/sq_mnt"
TMP_INIT="$AUDIT_DIR/initramfs"

mkdir -p "$MNT_ISO" "$MNT_SQ" "$TMP_INIT"

cleanup() {
    umount "$MNT_SQ" 2>/dev/null || true
    umount "$MNT_ISO" 2>/dev/null || true
    rm -rf "$AUDIT_DIR" 2>/dev/null || true
}
trap cleanup EXIT

echo "--- 1. Workspace Binaries (/workspace/build/bin) ---"
for b in tinexus-shell tinexus-dock tinexus-launcher tinexus-comp tinexus-serviced tinexus-hardware-probe; do
    bin="/workspace/build/bin/$b"
    if [ -f "$bin" ]; then
        sz=$(ls -lh "$bin" | awk '{print $5}')
        tx=$(strings "$bin" | grep -E "txui::|N4txui" -c || true)
        qt6="N/A"
        if strings "$bin" | grep "libQt6Core" >/dev/null 2>&1; then
            qt6="YES"
        fi
        echo "  [OK] $b: size=$sz, txui_refs=$tx, Qt6=$qt6"
    else
        echo "  [MISSING] $b does not exist!"
        exit 1
    fi
done

echo
echo "--- 2. ISO File Verification ---"
echo "  Path: $ISO"
echo "  Size: $(ls -lh "$ISO" | awk '{print $5}')"
echo "  SHA256: $(cat "$ISO.sha256" 2>/dev/null || sha256sum "$ISO")"

mount -o loop,ro "$ISO" "$MNT_ISO"

echo
echo "--- 3. ISO Initramfs Audit (/boot/initramfs.img) ---"
cp "$MNT_ISO/boot/initramfs.img" "$TMP_INIT/initramfs.img"
(cd "$TMP_INIT" && gzip -dc initramfs.img | cpio -idmv init 2>/dev/null)

echo "  switch_root command in /init:"
grep -n "switch_root" "$TMP_INIT/init" | sed 's/^/    /'

if grep "switch_root" "$TMP_INIT/init" | grep "tinexus-serviced" >/dev/null 2>&1; then
    echo "  [FAIL] ERROR: /init still calls tinexus-serviced directly!"
    exit 1
fi
if ! grep "switch_root" "$TMP_INIT/init" | grep "/sbin/init" >/dev/null 2>&1; then
    echo "  [FAIL] ERROR: /init does NOT target /sbin/init!"
    exit 1
fi
echo "  [PASS] switch_root targets /sbin/init (systemd PID 1)!"

echo
echo "--- 4. ISO Rootfs SquashFS Audit (/live/rootfs.squashfs) ---"
mount -o loop,ro "$MNT_ISO/live/rootfs.squashfs" "$MNT_SQ"

for b in tinexus-shell tinexus-dock tinexus-launcher tinexus-comp tinexus-serviced tinexus-hardware-probe; do
    bin="$MNT_SQ/usr/bin/$b"
    if [ -f "$bin" ]; then
        sz=$(ls -lh "$bin" | awk '{print $5}')
        tx=$(strings "$bin" | grep -E "txui::|N4txui" -c || true)
        qt6="N/A"
        if strings "$bin" | grep "libQt6Core" >/dev/null 2>&1; then
            qt6="YES"
        fi
        echo "  [OK] /usr/bin/$b: size=$sz, txui_refs=$tx, Qt6=$qt6"
        if [ "$tx" -gt 0 ]; then
            echo "  [FAIL] ERROR: $b inside ISO contains $tx txui references!"
            exit 1
        fi
    else
        echo "  [MISSING] /usr/bin/$b not found inside rootfs.squashfs!"
        exit 1
    fi
done

echo
echo "--- 5. Systemd Default Target & Enablement Inside Rootfs ---"
def_target="$(chroot "$MNT_SQ" systemctl get-default 2>/dev/null || true)"
echo "  Default target: $def_target"
if [ "$def_target" != "tinexus-session.target" ]; then
    echo "  [FAIL] ERROR: Default target is '$def_target', expected 'tinexus-session.target'!"
    exit 1
fi
echo "  [PASS] Default target is tinexus-session.target!"

for u in tinexus-session.target seatd.service tinexus-hardware-env.service tinexus-comp.service tinexus-serviced.service; do
    is_en="$(chroot "$MNT_SQ" systemctl is-enabled "$u" 2>/dev/null || true)"
    echo "  Unit $u: $is_en"
    if [ "$is_en" != "enabled" ]; then
        echo "  [FAIL] ERROR: Unit $u is not enabled (status: $is_en)!"
        exit 1
    fi
done
echo "  [PASS] All core Tinexus session units are enabled!"

echo
echo "--- 6. Dependency Graph of tinexus-session.target ---"
chroot "$MNT_SQ" systemctl list-dependencies --no-pager tinexus-session.target 2>/dev/null | head -25 | sed 's/^/    /'

echo
echo "--- 7. Terminal Console Configuration Audit ---"
if [ -f "$MNT_SQ/etc/systemd/system/serial-getty@ttyS0.service.d/autologin.conf" ]; then
    echo "  [PASS] Serial autologin configured on ttyS0 for diagnostics."
else
    echo "  [FAIL] ERROR: Serial autologin missing from ttyS0!"
    exit 1
fi
if [ ! -f "$MNT_SQ/etc/systemd/system/getty@tty1.service.d/autologin.conf" ]; then
    echo "  [PASS] getty@tty1 autologin is absent (graphical display owns tty1)."
else
    echo "  [FAIL] ERROR: getty@tty1 autologin is present and would overlap GUI!"
    exit 1
fi

echo
echo "========================================================"
echo "        ALL AUDIT GATES PASSED CLEANLY! ISO READY."
echo "========================================================"
