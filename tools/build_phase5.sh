#!/bin/bash
set -euo pipefail
WORKSPACE=/workspace
BUILD_DIR=$WORKSPACE/build
BUILD_TMP=/tmp/build_p5
PROTOS=$WORKSPACE/build/protocols
echo "=== Tinexus Phase 5 Production Build ==="
mkdir -p /mnt/isomnt /mnt/squashfs /mnt/rootfs /dev/shm/upper_p5 /dev/shm/work_p5
mount | grep -q /mnt/isomnt   || mount -o loop,ro "$BUILD_DIR/Tinexus-x86_64.iso" /mnt/isomnt
mount | grep -q /mnt/squashfs || mount -o loop,ro /mnt/isomnt/live/rootfs.squashfs /mnt/squashfs
mount | grep -q /mnt/rootfs   || mount -t overlay overlay -o lowerdir=/mnt/squashfs,upperdir=/dev/shm/upper_p5,workdir=/dev/shm/work_p5 /mnt/rootfs
echo nameserver 8.8.8.8 > /mnt/rootfs/etc/resolv.conf
mountpoint -q /mnt/rootfs/proc     || mount --bind /proc      /mnt/rootfs/proc
mountpoint -q /mnt/rootfs/sys      || mount --bind /sys       /mnt/rootfs/sys
mountpoint -q /mnt/rootfs/dev      || mount --bind /dev       /mnt/rootfs/dev
mkdir -p /mnt/rootfs/workspace
mountpoint -q /mnt/rootfs/workspace|| mount --bind /workspace /mnt/rootfs/workspace
mkdir -p /mnt/rootfs"$BUILD_TMP"
cleanup() {
    umount /mnt/rootfs/workspace 2>/dev/null||true
    umount /mnt/rootfs/dev 2>/dev/null||true
    umount /mnt/rootfs/sys 2>/dev/null||true
    umount /mnt/rootfs/proc 2>/dev/null||true
    umount /mnt/rootfs 2>/dev/null||true
    umount /mnt/squashfs 2>/dev/null||true
    umount /mnt/isomnt 2>/dev/null||true
    rm -rf /dev/shm/upper_p5 /dev/shm/work_p5 2>/dev/null||true
}
trap cleanup EXIT
[ -f /mnt/rootfs/usr/include/security/pam_appl.h ] || { chroot /mnt/rootfs /usr/bin/apt-get update; chroot /mnt/rootfs /usr/bin/apt-get install -y --no-install-recommends libpam0g-dev; }
echo "[OK] Chroot ready"
MOC_BIN=$(chroot /mnt/rootfs find /usr/lib/qt6 /usr/lib/x86_64-linux-gnu/qt6 /usr/bin -name moc -type f 2>/dev/null | head -1)
[ -z "$MOC_BIN" ] && MOC_BIN=/usr/bin/moc
QT6="-I/usr/include/x86_64-linux-gnu/qt6 -I/usr/include/x86_64-linux-gnu/qt6/QtCore -I/usr/include/x86_64-linux-gnu/qt6/QtGui -I/usr/include/x86_64-linux-gnu/qt6/QtQuick -I/usr/include/x86_64-linux-gnu/qt6/QtQml -I/usr/include/x86_64-linux-gnu/qt6/QtNetwork -I/usr/include/x86_64-linux-gnu/qt6/QtWaylandClient -I/usr/include/LayerShellQt -I$BUILD_TMP"
CI="-I/workspace/include -I/workspace/src -I/workspace/src/common/include -I/workspace/src/ipcd/include"
QL="-L/workspace/build/lib -L/workspace/build -L/usr/lib -L/usr/lib/x86_64-linux-gnu -ltinexus_common -lQt6Core -lQt6Gui -lQt6Quick -lQt6Qml -lQt6Network -lQt6WaylandClient -lLayerShellQtInterface"
CF="-std=c++20 -O2 -ftree-vectorize -Wall -Wextra -Wno-unused-parameter -fPIC"
echo ""; echo "=== [1/5] tinexus-comp ==="
bash "$WORKSPACE/tools/compile_comp.sh" 2>&1 | tail -5
cp -f "$BUILD_DIR/bin/tinexus-comp" "$BUILD_DIR/tinexus-comp" 2>/dev/null || true
ls -lh "$BUILD_DIR/bin/tinexus-comp"; echo "[OK] tinexus-comp"
echo ""; echo "=== [2/5] tinexus-launcher ==="
bash "$WORKSPACE/tools/compile_launcher.sh" 2>&1 | tail -5
cp -f "$BUILD_DIR/bin/tinexus-launcher" "$BUILD_DIR/tinexus-launcher" 2>/dev/null || true
ls -lh "$BUILD_DIR/bin/tinexus-launcher"; echo "[OK] tinexus-launcher"
echo ""; echo "=== [3/5] tinexus-shell ==="
bash "$WORKSPACE/tools/compile_shell.sh" 2>&1 | tail -5
cp -f "$BUILD_DIR/bin/tinexus-shell" "$BUILD_DIR/tinexus-shell" 2>/dev/null || true
ls -lh "$BUILD_DIR/bin/tinexus-shell"; echo "[OK] tinexus-shell"
echo ""; echo "=== [4/5] tinexus-wallpaper (Phase 5 M1-M7) ==="
chroot /mnt/rootfs /usr/bin/gcc -O2 -fPIC -I"$PROTOS" -I/usr/include/wayland "$PROTOS/wp-protocols-glue.c" -c -o "$BUILD_TMP/wp-protocols-glue.o"
chroot /mnt/rootfs /usr/bin/g++ $CF \
  -I/workspace/include -I/workspace/src -I/workspace/src/common/include -I/workspace/src/ipcd/include -I/workspace/src/wallpaper/include -I/workspace/src/txui/include -I"$PROTOS" -I/usr/include/pixman-1 -I/usr/include/freetype2 -I/usr/include/wayland \
  /workspace/src/wallpaper/main.cpp \
  /workspace/src/wallpaper/wallpaper_provider.cpp \
  /workspace/src/wallpaper/solar_schedule.cpp \
  "$BUILD_TMP/wp-protocols-glue.o" \
  -L/workspace/build/lib -L/workspace/build -L/usr/lib -L/usr/lib/x86_64-linux-gnu \
  -Wl,--whole-archive -ltxui -ltinexus_protocols_client -Wl,--no-whole-archive \
  -ltinexus_common -lwayland-client -lpixman-1 -lfreetype -lpthread \
  -o "$BUILD_TMP/tinexus-wallpaper"
cp -f /mnt/rootfs"$BUILD_TMP/tinexus-wallpaper" "$BUILD_DIR/tinexus-wallpaper"
cp -f /mnt/rootfs"$BUILD_TMP/tinexus-wallpaper" "$BUILD_DIR/bin/tinexus-wallpaper"
ls -lh "$BUILD_DIR/bin/tinexus-wallpaper"; echo "[OK] tinexus-wallpaper"
echo ""; echo "=== [5/5] tinexus-lock (Phase 5 M6) ==="
LI="-I/workspace/src/lock -I/workspace/src/lock/include $CI"
chroot /mnt/rootfs "$MOC_BIN" $QT6 $LI /workspace/src/lock/LockBridge.hpp -o "$BUILD_TMP/moc_LockBridge.cpp"
chroot /mnt/rootfs /usr/bin/g++ $CF $QT6 $LI -DTINEXUS_LOCK_HAS_PAM=1 -DHAVE_LAYERSHELL=1 \
  /workspace/src/lock/main.cpp /workspace/src/lock/LockBridge.cpp \
  $QL -lpam -o "$BUILD_TMP/tinexus-lock"
cp -f /mnt/rootfs"$BUILD_TMP/tinexus-lock" "$BUILD_DIR/tinexus-lock"
cp -f /mnt/rootfs"$BUILD_TMP/tinexus-lock" "$BUILD_DIR/bin/tinexus-lock"
ls -lh "$BUILD_DIR/bin/tinexus-lock"; echo "[OK] tinexus-lock"
echo ""; echo "=== VERIFICATION ==="
FAIL=0
for b in tinexus-comp tinexus-launcher tinexus-shell tinexus-wallpaper tinexus-lock; do
    f="$BUILD_DIR/bin/$b"
    if [ -f "$f" ]; then echo "  OK  $b  ($(stat -c%s $f) bytes)"; else echo "  FAIL $b"; FAIL=1; fi
done
[ "$FAIL" -eq 0 ] || { echo BUILD FAILED; exit 1; }
echo "All 5 binaries compiled successfully."