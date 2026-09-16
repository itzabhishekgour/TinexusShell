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

echo "[INFO] Generating MOC for tinexus-dock..."
chroot /mnt/rootfs "$MOC_BIN" /workspace/src/dock/DockBridge.hpp -o /tmp/build_apps/moc_DockBridge.cpp
chroot /mnt/rootfs "$MOC_BIN" /workspace/src/dock/include/dock/DockModel.hpp -o /tmp/build_apps/moc_DockModel.cpp
chroot /mnt/rootfs "$MOC_BIN" /workspace/src/dock/include/dock/ToplevelTracker.hpp -o /tmp/build_apps/moc_ToplevelTracker.cpp
chroot /mnt/rootfs "$MOC_BIN" /workspace/src/dock/include/dock/DockMenuPopup.hpp -o /tmp/build_apps/moc_DockMenuPopup.cpp
chroot /mnt/rootfs "$MOC_BIN" /workspace/src/dock/include/dock/DockWindow.hpp -o /tmp/build_apps/moc_DockWindow.cpp
chroot /mnt/rootfs "$MOC_BIN" /workspace/src/dock/include/dock/DnDHandler.hpp -o /tmp/build_apps/moc_DnDHandler.cpp
chroot /mnt/rootfs "$MOC_BIN" /workspace/src/dock/include/dock/StacksModel.hpp -o /tmp/build_apps/moc_StacksModel.cpp
chroot /mnt/rootfs "$MOC_BIN" /workspace/src/dock/include/dock/StacksPopup.hpp -o /tmp/build_apps/moc_StacksPopup.cpp
chroot /mnt/rootfs "$MOC_BIN" /workspace/src/dock/include/dock/DockIpcClient.hpp -o /tmp/build_apps/moc_DockIpcClient.cpp

echo "[INFO] Compiling Wayland Foreign Toplevel protocol glue..."
chroot /mnt/rootfs /usr/bin/gcc -O2 -fPIC -I/workspace/src/dock -I/usr/include/wayland \
  /workspace/src/dock/wayland/protocol/wlr-foreign-toplevel-management-unstable-v1-protocol.c -c -o /tmp/build_apps/wlr-foreign-toplevel.o

echo "[INFO] Compiling tinexus-dock (Slices 1-7)..."
chroot /mnt/rootfs /usr/bin/g++ -std=c++20 -O2 \
  $QT6_INC $COMMON_INC -I/workspace/src/dock/include -I/workspace/src/dock -I/usr/include/x86_64-linux-gnu/qt6/QtDBus \
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
  /tmp/build_apps/wlr-foreign-toplevel.o \
  -L/usr/lib/x86_64-linux-gnu -ltinexus_common -lQt6Core -lQt6Gui -lQt6Quick -lQt6Qml -lQt6Network -lQt6WaylandClient -lLayerShellQtInterface -lwayland-client -lQt6DBus -DHAVE_LAYERSHELL=1 \
  -o /workspace/build/bin/tinexus-dock

echo "[SUCCESS] tinexus-dock compiled successfully!"
ls -lh /workspace/build/bin/tinexus-dock

echo "[INFO] Compiling test-dock-render harness..."
chroot /mnt/rootfs /usr/bin/g++ -std=c++20 -O2 \
  $QT6_INC $COMMON_INC -I/workspace/src/dock/include -I/workspace/src/dock -I/usr/include/x86_64-linux-gnu/qt6/QtDBus \
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
  /tmp/build_apps/wlr-foreign-toplevel.o \
  -L/usr/lib/x86_64-linux-gnu -ltinexus_common -lQt6Core -lQt6Gui -lQt6Quick -lQt6Qml -lQt6Network -lQt6WaylandClient -lLayerShellQtInterface -lwayland-client -lQt6DBus -DHAVE_LAYERSHELL=1 \
  -o /workspace/build/bin/test-dock-render

echo "[SUCCESS] test-dock-render compiled successfully!"
ls -lh /workspace/build/bin/test-dock-render
