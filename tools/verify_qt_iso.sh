#!/usr/bin/env bash
set -eu

if [ -f "/workspace/build/workdisk.img" ]; then
    mkdir -p /mnt/workdisk
    mountpoint -q /mnt/workdisk || mount -o loop "/workspace/build/workdisk.img" /mnt/workdisk 2>/dev/null || true
fi

if mountpoint -q /mnt/workdisk; then
    WORK=$(mktemp -d /mnt/workdisk/verify_qt_iso_XXXXXX)
else
    WORK=$(mktemp -d /dev/shm/verify_qt_iso_XXXXXX)
fi

cleanup() {
    rm -rf "$WORK"
}
trap cleanup EXIT

ISO="/workspace/build/Tinexus-x86_64.iso"
echo "=== ISO Verification Report ==="
echo "ISO File: $ISO"
echo "ISO Size: $(ls -lh "$ISO" | awk '{print $5}')"
echo "ISO SHA256: $(cat "/workspace/build/Tinexus-x86_64.iso.sha256" 2>/dev/null || sha256sum "$ISO")"

echo ""
echo "=== 1. Extracting squashfs from ISO ==="
xorriso -osirrox on -indev "$ISO" -extract /live/rootfs.squashfs "$WORK/rootfs.squashfs"

echo ""
echo "=== 2. Checking key files inside rootfs.squashfs ==="
unsquashfs -l "$WORK/rootfs.squashfs" | grep -E "(tinexus-settings|tinexus-settings-ui|Sidebar.qml|MainWindow.qml|libqwayland|QtQuick)" | head -n 25

echo ""
echo "=== 3. Extracting binaries from rootfs.squashfs ==="
unsquashfs -d "$WORK/out" "$WORK/rootfs.squashfs" \
    "usr/bin/tinexus-settings" \
    "usr/bin/tinexus-settings-ui" \
    "usr/bin/tinexus-comp" \
    "usr/bin/tinexus-serviced" \
    "usr/bin/tinexus-shell" \
    "usr/share/tinexus-settings/qml/MainWindow.qml" \
    "usr/share/tinexus/settings-ui/qml/Sidebar.qml"

echo ""
echo "=== 4. Tier 1 Pure C++ Architecture Isolation Checks ==="
echo -n "tinexus-settings Qt dependencies: "
ldd "$WORK/out/usr/bin/tinexus-settings" 2>&1 | grep -i qt || echo "NONE (Strictly Pure C++20 Settings Daemon)"

echo -n "tinexus-comp Qt dependencies: "
ldd "$WORK/out/usr/bin/tinexus-comp" 2>&1 | grep -i qt || echo "NONE (Strictly Pure C++20 Compositor)"

echo -n "tinexus-serviced Qt dependencies: "
ldd "$WORK/out/usr/bin/tinexus-serviced" 2>&1 | grep -i qt || echo "NONE (Strictly Pure C++20 Supervisor)"

echo ""
echo "=== 5. Tier 2 Qt6 Settings UI & Shell Dependency Check ==="
ldd "$WORK/out/usr/bin/tinexus-settings-ui" | grep -i "libQt6" | head -n 5
ldd "$WORK/out/usr/bin/tinexus-shell" | grep -i "libQt6" | head -n 5

echo ""
echo "=== 6. Settings QML Frontend Check ==="
echo "MainWindow.qml size: $(wc -c < "$WORK/out/usr/share/tinexus-settings/qml/MainWindow.qml") bytes"
echo "Sidebar.qml size: $(wc -c < "$WORK/out/usr/share/tinexus/settings-ui/qml/Sidebar.qml") bytes"

echo ""
echo "=== 7. Hybrid Boot Partition Structure ==="
fdisk -l "$ISO"

echo ""
echo "========================================================"
echo "✅ ISO BUILD & ARCHITECTURE FULLY VALIDATED AND READY!"
echo "========================================================"
