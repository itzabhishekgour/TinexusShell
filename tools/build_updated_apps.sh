#!/bin/bash
set -euo pipefail

echo "=== 1. PREPARING MOUNTS ==="
mkdir -p /mnt/isomnt /mnt/squashfs /mnt/rootfs /dev/shm/upper_bld /dev/shm/work_bld

if ! mount | grep -q '/mnt/isomnt'; then
    mount -o loop,ro /workspace/build/Tinexus-x86_64.iso /mnt/isomnt
fi
if ! mount | grep -q '/mnt/squashfs'; then
    mount -o loop,ro /mnt/isomnt/live/rootfs.squashfs /mnt/squashfs
fi
if ! mount | grep -q '/mnt/rootfs'; then
    mount -t overlay overlay -o lowerdir=/mnt/squashfs,upperdir=/dev/shm/upper_bld,workdir=/dev/shm/work_bld /mnt/rootfs
fi

echo "nameserver 8.8.8.8" > /mnt/rootfs/etc/resolv.conf
mount --bind /proc /mnt/rootfs/proc 2>/dev/null || true
mount --bind /sys /mnt/rootfs/sys 2>/dev/null || true
mount --bind /dev /mnt/rootfs/dev 2>/dev/null || true
mkdir -p /mnt/rootfs/workspace
mount --bind /workspace /mnt/rootfs/workspace 2>/dev/null || true

cleanup() {
    echo "=== CLEANUP ==="
    umount /mnt/rootfs/workspace 2>/dev/null || true
    umount /mnt/rootfs/dev 2>/dev/null || true
    umount /mnt/rootfs/sys 2>/dev/null || true
    umount /mnt/rootfs/proc 2>/dev/null || true
    umount /mnt/rootfs 2>/dev/null || true
    umount /mnt/squashfs 2>/dev/null || true
    umount /mnt/isomnt 2>/dev/null || true
    rm -rf /dev/shm/upper_bld /dev/shm/work_bld 2>/dev/null || true
}
trap cleanup EXIT

echo "=== 2. INSTALLING BUILD TOOLS ==="
chroot /mnt/rootfs /usr/bin/apt-get update
chroot /mnt/rootfs /usr/bin/apt-get install -y --no-install-recommends \
  g++ qt6-base-dev-tools qt6-declarative-dev liblayershellqtinterface-dev libpam0g-dev

MOC_BIN="/usr/lib/qt6/libexec/moc"
[ -f "/mnt/rootfs$MOC_BIN" ] || MOC_BIN="/usr/bin/moc"

QT6_INC="-I/usr/include/x86_64-linux-gnu/qt6 -I/usr/include/x86_64-linux-gnu/qt6/QtCore -I/usr/include/x86_64-linux-gnu/qt6/QtGui -I/usr/include/x86_64-linux-gnu/qt6/QtQuick -I/usr/include/x86_64-linux-gnu/qt6/QtQml -I/usr/include/x86_64-linux-gnu/qt6/QtNetwork -I/usr/include/x86_64-linux-gnu/qt6/QtWaylandClient -I/usr/include/LayerShellQt -I/tmp/build_apps"
COMMON_INC="-I/workspace/include -I/workspace/src -I/workspace/src/common/include -I/workspace/src/ipcd/include"
QT6_LIBS="-L/usr/lib/x86_64-linux-gnu -ltinexus_common -lQt6Core -lQt6Gui -lQt6Quick -lQt6Qml -lQt6Network -lQt6WaylandClient -lLayerShellQtInterface -DHAVE_LAYERSHELL=1"

mkdir -p /mnt/rootfs/tmp/build_apps

echo "=== 3. COMPILING tinexus-dock ==="
chroot /mnt/rootfs "$MOC_BIN" /workspace/src/dock/DockBridge.hpp -o /tmp/build_apps/moc_DockBridge.cpp
chroot /mnt/rootfs "$MOC_BIN" /workspace/src/dock/include/dock/DockModel.hpp -o /tmp/build_apps/moc_DockModel.cpp
chroot /mnt/rootfs "$MOC_BIN" /workspace/src/dock/include/dock/ToplevelTracker.hpp -o /tmp/build_apps/moc_ToplevelTracker.cpp
chroot /mnt/rootfs "$MOC_BIN" /workspace/src/dock/include/dock/DockMenuPopup.hpp -o /tmp/build_apps/moc_DockMenuPopup.cpp
chroot /mnt/rootfs "$MOC_BIN" /workspace/src/dock/include/dock/DockWindow.hpp -o /tmp/build_apps/moc_DockWindow.cpp
chroot /mnt/rootfs "$MOC_BIN" /workspace/src/dock/include/dock/DnDHandler.hpp -o /tmp/build_apps/moc_DnDHandler.cpp
chroot /mnt/rootfs "$MOC_BIN" /workspace/src/dock/include/dock/StacksModel.hpp -o /tmp/build_apps/moc_StacksModel.cpp
chroot /mnt/rootfs "$MOC_BIN" /workspace/src/dock/include/dock/StacksPopup.hpp -o /tmp/build_apps/moc_StacksPopup.cpp
chroot /mnt/rootfs "$MOC_BIN" /workspace/src/dock/include/dock/DockIpcClient.hpp -o /tmp/build_apps/moc_DockIpcClient.cpp
chroot /mnt/rootfs /usr/bin/gcc -O2 -fPIC -I/workspace/src/dock -I/usr/include/wayland \
  /workspace/src/dock/wayland/protocol/wlr-foreign-toplevel-management-unstable-v1-protocol.c -c -o /tmp/build_apps/wlr-foreign-toplevel.o
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
  $QT6_LIBS -lwayland-client -lQt6DBus \
  -o /tmp/build_apps/tinexus-dock

echo "=== 4. COMPILING tinexus-shell ==="
chroot /mnt/rootfs "$MOC_BIN" /workspace/src/shell/ShellBridge.hpp -o /tmp/build_apps/moc_ShellBridge.cpp
chroot /mnt/rootfs /usr/bin/g++ -std=c++20 -O2 \
  $QT6_INC $COMMON_INC -I/workspace/src/shell -I/workspace/src/shell/include -I/usr/include/x86_64-linux-gnu/qt6/QtDBus \
  /workspace/src/shell/main.cpp \
  /workspace/src/shell/ShellBridge.cpp \
  $QT6_LIBS -lQt6DBus -lQt6Svg \
  -o /tmp/build_apps/tinexus-shell

echo "=== 5. COMPILING tinexus-lock ==="
chroot /mnt/rootfs "$MOC_BIN" /workspace/src/lock/LockBridge.hpp -o /tmp/build_apps/moc_LockBridge.cpp
chroot /mnt/rootfs /usr/bin/g++ -std=c++20 -O2 \
  $QT6_INC $COMMON_INC -I/workspace/src/lock -I/workspace/src/lock/include \
  /workspace/src/lock/main.cpp \
  /workspace/src/lock/LockBridge.cpp \
  $QT6_LIBS -lpam \
  -o /tmp/build_apps/tinexus-lock

echo "=== 6. DEPLOYING TO /workspace/build/bin ==="
ls -lh /mnt/rootfs/tmp/build_apps/tinexus-dock /mnt/rootfs/tmp/build_apps/tinexus-shell /mnt/rootfs/tmp/build_apps/tinexus-lock
cp -f /mnt/rootfs/tmp/build_apps/tinexus-dock /workspace/build/bin/tinexus-dock
cp -f /mnt/rootfs/tmp/build_apps/tinexus-shell /workspace/build/bin/tinexus-shell
cp -f /mnt/rootfs/tmp/build_apps/tinexus-lock /workspace/build/bin/tinexus-lock

echo "=== SUCCESS: Updated binaries compiled and copied to build/bin! ==="
