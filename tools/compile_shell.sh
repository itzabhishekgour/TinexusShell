#!/bin/bash
set -euo pipefail

ORIG_ISO="/workspace/build/Tinexus-x86_64.iso"

# Check if /mnt/rootfs is already mounted
if ! mountpoint -q /mnt/rootfs; then
    if [ -f "/workspace/build/workdisk.img" ]; then
        mkdir -p /mnt/workdisk
        mountpoint -q /mnt/workdisk || mount -o loop "/workspace/build/workdisk.img" /mnt/workdisk 2>/dev/null || true
    fi
    if mountpoint -q /mnt/workdisk; then
        UPPER_DIR="/mnt/workdisk/shell_upper"
        WORK_DIR="/mnt/workdisk/shell_work"
    else
        UPPER_DIR="/var/tmp/shell_upper"
        WORK_DIR="/var/tmp/shell_work"
    fi
    mkdir -p /mnt/isomnt /mnt/squashfs /mnt/rootfs "$UPPER_DIR" "$WORK_DIR"
    mountpoint -q /mnt/isomnt || mount -o loop,ro "$ORIG_ISO" /mnt/isomnt
    mountpoint -q /mnt/squashfs || mount -o loop,ro /mnt/isomnt/live/rootfs.squashfs /mnt/squashfs
    mount -t overlay overlay -o lowerdir=/mnt/squashfs,upperdir="$UPPER_DIR",workdir="$WORK_DIR" /mnt/rootfs
fi

mountpoint -q /mnt/rootfs/proc || mount --bind /proc /mnt/rootfs/proc 2>/dev/null || true
mountpoint -q /mnt/rootfs/sys || mount --bind /sys /mnt/rootfs/sys 2>/dev/null || true
mountpoint -q /mnt/rootfs/dev || mount --bind /dev /mnt/rootfs/dev 2>/dev/null || true
mkdir -p /mnt/rootfs/workspace
mountpoint -q /mnt/rootfs/workspace || mount --bind /workspace /mnt/rootfs/workspace 2>/dev/null || true

if mountpoint -q /mnt/workdisk; then
    mkdir -p /mnt/workdisk/tmp
    chmod 1777 /mnt/workdisk/tmp
    mkdir -p /mnt/rootfs/tmp
    mountpoint -q /mnt/rootfs/tmp || mount --bind /mnt/workdisk/tmp /mnt/rootfs/tmp 2>/dev/null || true
fi
mkdir -p /mnt/rootfs/tmp/build_apps
rm -rf /mnt/rootfs/tmp/build_apps/*
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

QT6_INC="-I/usr/include/x86_64-linux-gnu/qt6 -I/usr/include/x86_64-linux-gnu/qt6/QtCore -I/usr/include/x86_64-linux-gnu/qt6/QtGui -I/usr/include/x86_64-linux-gnu/qt6/QtQuick -I/usr/include/x86_64-linux-gnu/qt6/QtQml -I/usr/include/x86_64-linux-gnu/qt6/QtNetwork -I/usr/include/x86_64-linux-gnu/qt6/QtWaylandClient -I/usr/include/x86_64-linux-gnu/qt6/QtSvg -I/usr/include/LayerShellQt -I/tmp/build_apps"
COMMON_INC="-I/workspace/include -I/workspace/src -I/workspace/src/common/include -I/workspace/src/ipcd/include"

echo "[INFO] Running moc on ShellBridge.hpp..."
chroot /mnt/rootfs "$MOC_BIN" /workspace/src/shell/ShellBridge.hpp -o /tmp/build_apps/moc_ShellBridge.cpp

echo "[INFO] Compiling tinexus-shell with TinexusIconProvider and Inter typography..."
mkdir -p /workspace/build/bin
rm -f /workspace/build/bin/tinexus-shell
BUILD_START_TIME=$(date +%s)

chroot /mnt/rootfs /usr/bin/g++ -std=c++20 -O2 \
  $QT6_INC $COMMON_INC -I/workspace/src/shell -I/workspace/src/shell/include -I/usr/include/x86_64-linux-gnu/qt6/QtDBus \
  /workspace/src/shell/main.cpp \
  /workspace/src/shell/ShellBridge.cpp \
  -L/workspace/build/lib -L/usr/lib -L/usr/lib/x86_64-linux-gnu \
  -ltinexus_common -lQt6Core -lQt6Gui -lQt6Quick -lQt6Qml -lQt6Network -lQt6WaylandClient -lLayerShellQtInterface -lQt6DBus -lQt6Svg -DHAVE_LAYERSHELL=1 \
  -o /workspace/build/bin/tinexus-shell

# Rigorous post-build assertions
if [ ! -f /workspace/build/bin/tinexus-shell ]; then
    echo "[FATAL] tinexus-shell compilation failed: binary does not exist!" >&2
    exit 1
fi
bin_mtime=$(stat -c %Y /workspace/build/bin/tinexus-shell)
if [ "$bin_mtime" -lt "$BUILD_START_TIME" ]; then
    echo "[FATAL] tinexus-shell binary mtime ($bin_mtime) is older than build start ($BUILD_START_TIME)! Stale binary." >&2
    exit 1
fi
if strings /workspace/build/bin/tinexus-shell | grep -E "txui::|N4txui" >/dev/null 2>&1; then
    echo "[FATAL] tinexus-shell contains txui references! Legacy binary detected." >&2
    exit 1
fi
if ! strings /workspace/build/bin/tinexus-shell | grep "libQt6Core" >/dev/null 2>&1; then
    echo "[FATAL] tinexus-shell is NOT linked against Qt6!" >&2
    exit 1
fi

echo "[SUCCESS] tinexus-shell compiled and verified successfully ($(ls -lh /workspace/build/bin/tinexus-shell | awk '{print $5}'))"

echo "[INFO] Compiling test-shell-render harness..."
chroot /mnt/rootfs /usr/bin/g++ -std=c++20 -O2 \
  $QT6_INC $COMMON_INC -I/workspace/src/shell -I/workspace/src/shell/include -I/usr/include/x86_64-linux-gnu/qt6/QtDBus \
  /workspace/src/shell/test_shell_render.cpp \
  /workspace/src/shell/ShellBridge.cpp \
  -L/workspace/build/lib -L/usr/lib -L/usr/lib/x86_64-linux-gnu \
  -ltinexus_common -lQt6Core -lQt6Gui -lQt6Quick -lQt6Qml -lQt6Network -lQt6DBus -lQt6Svg \
  -o /workspace/build/bin/test-shell-render

[ -f /workspace/build/bin/test-shell-render ] || { echo "[FATAL] test-shell-render failed!" >&2; exit 1; }
echo "[SUCCESS] test-shell-render compiled successfully!"
