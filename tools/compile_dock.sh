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
        UPPER_DIR="/mnt/workdisk/dock_upper"
        WORK_DIR="/mnt/workdisk/dock_work"
    else
        UPPER_DIR="/var/tmp/dock_upper"
        WORK_DIR="/var/tmp/dock_work"
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

QT6_INC="-I/usr/include/x86_64-linux-gnu/qt6 -I/usr/include/x86_64-linux-gnu/qt6/QtCore -I/usr/include/x86_64-linux-gnu/qt6/QtGui -I/usr/include/x86_64-linux-gnu/qt6/QtQuick -I/usr/include/x86_64-linux-gnu/qt6/QtQml -I/usr/include/x86_64-linux-gnu/qt6/QtNetwork -I/usr/include/x86_64-linux-gnu/qt6/QtWaylandClient -I/usr/include/LayerShellQt -I/tmp/build_apps"
COMMON_INC="-I/workspace/include -I/workspace/src -I/workspace/src/common/include -I/workspace/src/ipcd/include"

echo "[INFO] Generating MOC for tinexus-dock..."
chroot /mnt/rootfs "$MOC_BIN" $COMMON_INC /workspace/src/dock/include/dock/DockBridge.hpp -o /tmp/build_apps/moc_DockBridge.cpp
chroot /mnt/rootfs "$MOC_BIN" $COMMON_INC /workspace/src/dock/include/dock/DockModel.hpp -o /tmp/build_apps/moc_DockModel.cpp
chroot /mnt/rootfs "$MOC_BIN" $COMMON_INC /workspace/src/dock/include/dock/ToplevelTracker.hpp -o /tmp/build_apps/moc_ToplevelTracker.cpp
chroot /mnt/rootfs "$MOC_BIN" $COMMON_INC /workspace/src/dock/include/dock/DockMenuPopup.hpp -o /tmp/build_apps/moc_DockMenuPopup.cpp
chroot /mnt/rootfs "$MOC_BIN" $COMMON_INC /workspace/src/dock/include/dock/DockWindow.hpp -o /tmp/build_apps/moc_DockWindow.cpp
chroot /mnt/rootfs "$MOC_BIN" $COMMON_INC /workspace/src/dock/include/dock/DnDHandler.hpp -o /tmp/build_apps/moc_DnDHandler.cpp
chroot /mnt/rootfs "$MOC_BIN" $COMMON_INC /workspace/src/dock/include/dock/StacksModel.hpp -o /tmp/build_apps/moc_StacksModel.cpp
chroot /mnt/rootfs "$MOC_BIN" $COMMON_INC /workspace/src/dock/include/dock/StacksPopup.hpp -o /tmp/build_apps/moc_StacksPopup.cpp
chroot /mnt/rootfs "$MOC_BIN" $COMMON_INC /workspace/src/dock/include/dock/DockIpcClient.hpp -o /tmp/build_apps/moc_DockIpcClient.cpp
chroot /mnt/rootfs "$MOC_BIN" $COMMON_INC /workspace/src/dock/include/dock/DockAdaptor.hpp -o /tmp/build_apps/moc_DockAdaptor.cpp

echo "[INFO] Compiling Wayland Foreign Toplevel protocol glue..."
chroot /mnt/rootfs /usr/bin/gcc -O2 -fPIC -I/workspace/src/dock -I/usr/include/wayland \
  /workspace/src/dock/wayland/protocol/wlr-foreign-toplevel-management-unstable-v1-protocol.c -c -o /tmp/build_apps/wlr-foreign-toplevel.o

echo "[INFO] Compiling tinexus-dock (Slices 1-7)..."
mkdir -p /workspace/build/bin
rm -f /workspace/build/bin/tinexus-dock
BUILD_START_TIME=$(date +%s)

chroot /mnt/rootfs /usr/bin/g++ -std=c++20 -O2 \
  $QT6_INC $COMMON_INC -I/workspace/src/dock/include -I/workspace/src/dock -I/workspace/src/files/include -I/usr/include/x86_64-linux-gnu/qt6/QtDBus \
  /workspace/src/dock/main.cpp \
  /workspace/src/dock/DockBridge.cpp \
  /workspace/src/dock/DockModel.cpp \
  /workspace/src/dock/ToplevelTracker.cpp \
  /workspace/src/dock/DockMenuPopup.cpp \
  /workspace/src/dock/DockWindow.cpp \
  /workspace/src/dock/DnDHandler.cpp \
  /workspace/src/dock/StacksModel.cpp \
  /workspace/src/dock/StacksPopup.cpp \
  /workspace/src/dock/DockIpcClient.cpp \
  /workspace/src/dock/DockAdaptor.cpp \
  /workspace/src/files/trash_manager.cpp \
  /tmp/build_apps/wlr-foreign-toplevel.o \
  -L/usr/lib/x86_64-linux-gnu -ltinexus_common -lQt6Core -lQt6Gui -lQt6Quick -lQt6Qml -lQt6Network -lQt6WaylandClient -lLayerShellQtInterface -lwayland-client -lQt6DBus -DHAVE_LAYERSHELL=1 \
  -o /workspace/build/bin/tinexus-dock

# Rigorous post-build assertions
if [ ! -f /workspace/build/bin/tinexus-dock ]; then
    echo "[FATAL] tinexus-dock compilation failed: binary does not exist!" >&2
    exit 1
fi
bin_mtime=$(stat -c %Y /workspace/build/bin/tinexus-dock)
if [ "$bin_mtime" -lt "$BUILD_START_TIME" ]; then
    echo "[FATAL] tinexus-dock binary mtime ($bin_mtime) is older than build start ($BUILD_START_TIME)! Stale binary." >&2
    exit 1
fi
if strings /workspace/build/bin/tinexus-dock | grep -E "txui::|N4txui" >/dev/null 2>&1; then
    echo "[FATAL] tinexus-dock contains txui references! Legacy binary detected." >&2
    exit 1
fi
if ! strings /workspace/build/bin/tinexus-dock | grep "libQt6Core" >/dev/null 2>&1; then
    echo "[FATAL] tinexus-dock is NOT linked against Qt6!" >&2
    exit 1
fi

echo "[SUCCESS] tinexus-dock compiled and verified successfully ($(ls -lh /workspace/build/bin/tinexus-dock | awk '{print $5}'))"

echo "[INFO] Compiling test-dock-render harness..."
chroot /mnt/rootfs /usr/bin/g++ -std=c++20 -O2 \
  $QT6_INC $COMMON_INC -I/workspace/src/dock/include -I/workspace/src/dock -I/workspace/src/files/include -I/usr/include/x86_64-linux-gnu/qt6/QtDBus \
  /workspace/src/dock/test_dock_render.cpp \
  /workspace/src/dock/DockBridge.cpp \
  /workspace/src/dock/DockModel.cpp \
  /workspace/src/dock/ToplevelTracker.cpp \
  /workspace/src/dock/DockMenuPopup.cpp \
  /workspace/src/dock/DockWindow.cpp \
  /workspace/src/dock/DnDHandler.cpp \
  /workspace/src/dock/StacksModel.cpp \
  /workspace/src/dock/StacksPopup.cpp \
  /workspace/src/dock/DockIpcClient.cpp \
  /workspace/src/dock/DockAdaptor.cpp \
  /workspace/src/files/trash_manager.cpp \
  /tmp/build_apps/wlr-foreign-toplevel.o \
  -L/usr/lib/x86_64-linux-gnu -ltinexus_common -lQt6Core -lQt6Gui -lQt6Quick -lQt6Qml -lQt6Network -lQt6WaylandClient -lLayerShellQtInterface -lwayland-client -lQt6DBus -DHAVE_LAYERSHELL=1 \
  -o /workspace/build/bin/test-dock-render

[ -f /workspace/build/bin/test-dock-render ] || { echo "[FATAL] test-dock-render failed!" >&2; exit 1; }

echo "[INFO] Syncing QML files to rootfs..."
mkdir -p /mnt/rootfs/usr/share/tinexus/dock/qml /mnt/rootfs/usr/share/tinexus-dock/qml
cp -r /workspace/src/dock/qml/* /mnt/rootfs/usr/share/tinexus/dock/qml/
cp -r /workspace/src/dock/qml/* /mnt/rootfs/usr/share/tinexus-dock/qml/

echo "[SUCCESS] test-dock-render compiled successfully!"
