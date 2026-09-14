#!/bin/bash
set -euo pipefail

ORIG_ISO="/workspace/build/Tinexus-x86_64.iso"
mkdir -p /mnt/isomnt /mnt/squashfs /mnt/rootfs /dev/shm/upper /dev/shm/work

if ! mount | grep -q '/mnt/isomnt'; then
    echo "[INFO] Mounting original ISO to /mnt/isomnt..."
    mount -o loop,ro "$ORIG_ISO" /mnt/isomnt
fi

if ! mount | grep -q '/mnt/squashfs'; then
    echo "[INFO] Mounting squashfs to /mnt/squashfs..."
    mount -o loop,ro /mnt/isomnt/live/rootfs.squashfs /mnt/squashfs
fi

if ! mount | grep -q '/mnt/rootfs'; then
    echo "[INFO] Mounting overlayfs to /mnt/rootfs..."
    mount -t overlay overlay -o lowerdir=/mnt/squashfs,upperdir=/dev/shm/upper,workdir=/dev/shm/work /mnt/rootfs
fi

mountpoint -q /mnt/rootfs/proc || mount --bind /proc /mnt/rootfs/proc 2>/dev/null || true
mountpoint -q /mnt/rootfs/sys || mount --bind /sys /mnt/rootfs/sys 2>/dev/null || true
mountpoint -q /mnt/rootfs/dev || mount --bind /dev /mnt/rootfs/dev 2>/dev/null || true
mkdir -p /mnt/rootfs/workspace
mountpoint -q /mnt/rootfs/workspace || mount --bind /workspace /mnt/rootfs/workspace 2>/dev/null || true

mkdir -p /mnt/rootfs/tmp/build_apps
chmod 1777 /mnt/rootfs/tmp /mnt/rootfs/tmp/build_apps 2>/dev/null || true

MOC_BIN="/usr/lib/qt6/libexec/moc"
if [ ! -f "/mnt/rootfs$MOC_BIN" ]; then
    if [ -f "/mnt/rootfs/usr/lib/x86_64-linux-gnu/qt6/libexec/moc" ]; then
        MOC_BIN="/usr/lib/x86_64-linux-gnu/qt6/libexec/moc"
    elif [ -f "/mnt/rootfs/usr/bin/moc" ]; then
        MOC_BIN="/usr/bin/moc"
    else
        MOC_BIN="$(chroot /mnt/rootfs which moc || true)"
    fi
fi

echo "[INFO] Found moc at: $MOC_BIN"

QT6_INC="-I/usr/include/x86_64-linux-gnu/qt6 -I/usr/include/x86_64-linux-gnu/qt6/QtCore -I/usr/include/x86_64-linux-gnu/qt6/QtGui -I/usr/include/x86_64-linux-gnu/qt6/QtQuick -I/usr/include/x86_64-linux-gnu/qt6/QtQml -I/usr/include/x86_64-linux-gnu/qt6/QtNetwork -I/usr/include/x86_64-linux-gnu/qt6/QtWaylandClient -I/usr/include/LayerShellQt -I/tmp/build_apps"
COMMON_INC="-I/workspace/include -I/workspace/src -I/workspace/src/common/include -I/workspace/src/ipcd/include"

echo "[INFO] Running moc on LauncherBridge.hpp..."
chroot /mnt/rootfs "$MOC_BIN" /workspace/src/launcher/LauncherBridge.hpp -o /tmp/build_apps/moc_LauncherBridge.cpp

echo "[INFO] Compiling tinexus-launcher..."
chroot /mnt/rootfs /usr/bin/g++ -std=c++20 -O2 \
  $QT6_INC $COMMON_INC -I/workspace/src/indexer/include -I/workspace/src/launcher -I/workspace/src/launcher/include \
  /workspace/src/launcher/main.cpp \
  /workspace/src/launcher/LauncherBridge.cpp \
  -L/workspace/build/lib -L/usr/lib/x86_64-linux-gnu -ltinexus_common -lQt6Core -lQt6Gui -lQt6Quick -lQt6Qml -lQt6Network -lQt6WaylandClient -lLayerShellQtInterface -DHAVE_LAYERSHELL=1 \
  -o /workspace/build/bin/tinexus-launcher

echo "[INFO] Compiling test-launcher-render..."
chroot /mnt/rootfs /usr/bin/g++ -std=c++20 -O2 \
  $QT6_INC $COMMON_INC -I/workspace/src/indexer/include -I/workspace/src/launcher -I/workspace/src/launcher/include \
  /workspace/src/launcher/test_launcher_render.cpp \
  /workspace/src/launcher/LauncherBridge.cpp \
  -L/workspace/build/lib -L/usr/lib/x86_64-linux-gnu -ltinexus_common -lQt6Core -lQt6Gui -lQt6Quick -lQt6Qml -lQt6Network -lQt6WaylandClient \
  -o /workspace/build/bin/test-launcher-render

echo "[INFO] Running test-launcher-render harness..."
chroot /mnt/rootfs env QT_QPA_PLATFORM=offscreen QSG_RHI_BACKEND=software /workspace/build/bin/test-launcher-render

echo "[SUCCESS] tinexus-launcher compiled and verified!"
