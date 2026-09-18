#!/bin/bash
set -euo pipefail

ORIG_ISO="/workspace/build/Tinexus-x86_64.iso"
if [ -f "/workspace/build/workdisk.img" ]; then
    mkdir -p /mnt/workdisk
    mountpoint -q /mnt/workdisk || mount -o loop "/workspace/build/workdisk.img" /mnt/workdisk 2>/dev/null || true
fi
if mountpoint -q /mnt/workdisk; then
    UPPER_DIR="/mnt/workdisk/comp_upper"
    WORK_DIR="/mnt/workdisk/comp_work"
else
    UPPER_DIR="/var/tmp/comp_upper"
    WORK_DIR="/var/tmp/comp_work"
fi
mkdir -p /mnt/isomnt /mnt/squashfs /mnt/rootfs "$UPPER_DIR" "$WORK_DIR"

if ! mountpoint -q /mnt/isomnt; then
    echo "[INFO] Mounting original ISO to /mnt/isomnt..."
    mount -o loop,ro "$ORIG_ISO" /mnt/isomnt
fi

if ! mountpoint -q /mnt/squashfs; then
    echo "[INFO] Mounting squashfs to /mnt/squashfs..."
    mount -o loop,ro /mnt/isomnt/live/rootfs.squashfs /mnt/squashfs
fi

if ! mountpoint -q /mnt/rootfs; then
    echo "[INFO] Mounting overlayfs to /mnt/rootfs..."
    mount -t overlay overlay -o lowerdir=/mnt/squashfs,upperdir="$UPPER_DIR",workdir="$WORK_DIR" /mnt/rootfs
fi

mountpoint -q /mnt/rootfs/proc || mount --bind /proc /mnt/rootfs/proc 2>/dev/null || true
mountpoint -q /mnt/rootfs/sys || mount --bind /sys /mnt/rootfs/sys 2>/dev/null || true
mountpoint -q /mnt/rootfs/dev || mount --bind /dev /mnt/rootfs/dev 2>/dev/null || true
mkdir -p /mnt/rootfs/workspace
mountpoint -q /mnt/rootfs/workspace || mount --bind /workspace /mnt/rootfs/workspace 2>/dev/null || true

echo "[INFO] Generating wayland protocols including org-kde-kwin-blur..."
mkdir -p /mnt/rootfs/workspace/build/protocols
chroot /mnt/rootfs wayland-scanner server-header /workspace/protocols/org-kde-kwin-blur.xml /workspace/build/protocols/org-kde-kwin-blur-protocol.h
chroot /mnt/rootfs wayland-scanner private-code /workspace/protocols/org-kde-kwin-blur.xml /workspace/build/protocols/org-kde-kwin-blur-protocol.c

echo "[INFO] Compiling tinexus-comp with BlurManager & TinexusDecorationManager..."
COMP_SRC="/workspace/src/comp"
OBJ_DIR="/mnt/rootfs/workspace/build/comp_objs"
mkdir -p "$OBJ_DIR"

INCLUDES=(
  -I/workspace/src/comp
  -I/workspace/src/comp/server/include
  -I/workspace/src/comp/backend/include
  -I/workspace/src/comp/output/include
  -I/workspace/src/comp/cursor/include
  -I/workspace/src/comp/window/include
  -I/workspace/src/comp/focus/include
  -I/workspace/src/comp/workspace/include
  -I/workspace/src/comp/shell/include
  -I/workspace/src/comp/render/include
  -I/workspace/src/comp/animation/include
  -I/workspace/src/comp/input/include
  -I/workspace/src/comp/surface/include
  -I/workspace/include
  -I/workspace/src
  -I/workspace/src/common/include
  -I/workspace/src/ipcd/include
  -I/workspace/build/protocols
  -I/usr/include/pixman-1
  -I/usr/include/libdrm
  -I/usr/include/wlroots-0.19
  -DWLR_USE_UNSTABLE=1
)

SRCS=(
  $COMP_SRC/main.cpp
  $COMP_SRC/server/server.cpp
  $COMP_SRC/server/global_registry.cpp
  $COMP_SRC/server/surface_tree.cpp
  $COMP_SRC/backend/headless_backend.cpp
  $COMP_SRC/backend/wlroots_backend.cpp
  $COMP_SRC/backend/drm_backend.cpp
  $COMP_SRC/surface/surface_state.cpp
  $COMP_SRC/surface/configure_serial.cpp
  $COMP_SRC/surface/buffer_manager.cpp
  $COMP_SRC/surface/frame_callback.cpp
  $COMP_SRC/surface/resource_cleanup.cpp
  $COMP_SRC/window/decoration_manager.cpp
  $COMP_SRC/input/keymap_engine.cpp
  $COMP_SRC/input/interaction_controller.cpp
  $COMP_SRC/input/seat_manager.cpp
  $COMP_SRC/input/udev_monitor.cpp
  $COMP_SRC/output/output.cpp
  $COMP_SRC/output/output_layout.cpp
  $COMP_SRC/output/output_manager.cpp
  $COMP_SRC/cursor/cursor_manager.cpp
  $COMP_SRC/window/window_rules.cpp
  $COMP_SRC/focus/focus_manager.cpp
  $COMP_SRC/workspace/workspace_manager.cpp
  $COMP_SRC/shell/shell_state.cpp
  $COMP_SRC/render/frame_scheduler.cpp
  $COMP_SRC/animation/animation.cpp
  $COMP_SRC/animation/animation_manager.cpp
  $COMP_SRC/input/shortcut_engine.cpp
  $COMP_SRC/surface/surface_manager.cpp
  $COMP_SRC/surface/layer_shell_manager.cpp
  $COMP_SRC/surface/blur_manager.cpp
  $COMP_SRC/surface/exclusive_zone_calculator.cpp
  /workspace/build/protocols/org-kde-kwin-blur-protocol.c
  $COMP_SRC/server/protocol_dispatcher.cpp
  $COMP_SRC/render/damage_tracker.cpp
)

MAX_JOBS=8
OBJS=()

rm -f "$OBJ_DIR/org-kde-kwin-blur-protocol.o" "$OBJ_DIR/blur_manager.o"

for src in "${SRCS[@]}"; do
  bname=$(basename "$src")
  stem="${bname%.*}"
  obj_chroot="/workspace/build/comp_objs/${stem}.o"
  obj_host="/mnt/rootfs${obj_chroot}"
  src_host="/mnt/rootfs${src}"
  OBJS+=("$obj_chroot")

  if [ -f "$obj_host" ] && [ "$obj_host" -nt "$src_host" ]; then
    continue
  fi

  while [ $(jobs -rp | wc -l) -ge $MAX_JOBS ]; do
    sleep 0.05
  done

  if [[ "$src" == *.c ]]; then
    echo "  [CC]  $bname"
    chroot /mnt/rootfs /usr/bin/gcc -fPIC -fPIE -I/workspace/build/protocols -I/usr/include -c "$src" -o "$obj_chroot" &
  else
    echo "  [CXX] $bname"
    chroot /mnt/rootfs /usr/bin/g++ -std=c++20 -O2 -fPIE "${INCLUDES[@]}" -c "$src" -o "$obj_chroot" &
  fi
done

wait

echo "[INFO] Linking tinexus-comp..."
mkdir -p /workspace/build/bin
rm -f /workspace/build/bin/tinexus-comp
BUILD_START_TIME=$(date +%s)

chroot /mnt/rootfs /usr/bin/g++ -std=c++20 -O2 \
  "${OBJS[@]}" \
  -L/workspace/build/lib -L/usr/lib -L/usr/lib/x86_64-linux-gnu \
  -ltinexus_common -ltinexus_protocols -lwayland-server -lwlroots-0.19 -ldrm -lvulkan -lxkbcommon -lpixman-1 -lsystemd -lpthread \
  -o /workspace/build/bin/tinexus-comp

# Post-link assertions
if [ ! -f /workspace/build/bin/tinexus-comp ]; then
    echo "[FATAL] tinexus-comp linking failed: binary does not exist!" >&2
    exit 1
fi
bin_mtime=$(stat -c %Y /workspace/build/bin/tinexus-comp)
if [ "$bin_mtime" -lt "$BUILD_START_TIME" ]; then
    echo "[FATAL] tinexus-comp binary mtime ($bin_mtime) is older than build start ($BUILD_START_TIME)! Stale binary." >&2
    exit 1
fi

echo "[SUCCESS] tinexus-comp compiled successfully! ($(ls -lh /workspace/build/bin/tinexus-comp | awk '{print $5}'))"

