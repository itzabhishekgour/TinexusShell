#!/bin/bash
set -euo pipefail

PROJECT_DIR="/workspace"
cd "$PROJECT_DIR"
/workspace/tools/ensure_mounts.sh

killall -9 tinexus-comp tinexus-shell firefox 2>/dev/null || true
rm -rf /mnt/rootfs/tmp/tinexus-shell-fs-test
mkdir -p /mnt/rootfs/tmp/tinexus-shell-fs-test
chmod 700 /mnt/rootfs/tmp/tinexus-shell-fs-test

COMP_LOG="/workspace/build/comp_shell_fs.log"
SHELL_LOG="/workspace/build/shell_fs.log"
rm -f "$COMP_LOG" "$SHELL_LOG"

echo "[1] Starting tinexus-comp..."
chroot /mnt/rootfs /bin/bash -c "
  export XDG_RUNTIME_DIR=/tmp/tinexus-shell-fs-test
  export WAYLAND_DISPLAY=wayland-0
  export WLR_BACKENDS=headless
  export WLR_HEADLESS_OUTPUTS=1
  export WLR_LIBINPUT_NO_DEVICES=1
  export WLR_RENDERER=pixman
  /workspace/build/bin/tinexus-comp > /workspace/build/comp_shell_fs.log 2>&1
" &
COMP_PID=$!

FIFO_PATH="/mnt/rootfs/tmp/tinexus-shell-fs-test/tinexus/comp-cmd.fifo"

READY=0
for i in {1..30}; do
    if [ -S /mnt/rootfs/tmp/tinexus-shell-fs-test/wayland-0 ] && [ -p "$FIFO_PATH" ]; then
        echo "[+] Compositor ready!"
        READY=1
        break
    fi
    sleep 0.2
done

if [ $READY -ne 1 ]; then
    echo "[!] Compositor failed to start"
    exit 1
fi

echo "[2] Starting tinexus-shell (TopBar on LayerTop)..."
chroot /mnt/rootfs /bin/bash -c "
  export XDG_RUNTIME_DIR=/tmp/tinexus-shell-fs-test
  export WAYLAND_DISPLAY=wayland-0
  export QT_QPA_PLATFORM=wayland
  export LIBGL_ALWAYS_SOFTWARE=1
  /workspace/build/bin/tinexus-shell > /workspace/build/shell_fs.log 2>&1
" &
SHELL_PID=$!

for i in {1..30}; do
    if grep -q "tinexus-panel" "$COMP_LOG" 2>/dev/null || grep -q "Created scene layer surface" "$COMP_LOG" 2>/dev/null; then
        echo "[+] tinexus-shell TopBar successfully mapped onto LayerTop!"
        break
    fi
    sleep 0.3
done

sleep 1
echo "[3] Taking screenshot before fullscreen (TopBar should be visible)..."
echo "screenshot /workspace/build/desktop_with_topbar.ppm" > "$FIFO_PATH"
sleep 0.5

echo "[4] Launching Firefox..."
chroot /mnt/rootfs /bin/bash -c "
  export XDG_RUNTIME_DIR=/tmp/tinexus-shell-fs-test
  export WAYLAND_DISPLAY=wayland-0
  export MOZ_ENABLE_WAYLAND=1
  export MOZ_SANDBOX=0
  export MOZ_DISABLE_CONTENT_SANDBOX=1
  export LIBGL_ALWAYS_SOFTWARE=1
  export GDK_BACKEND=wayland
  export NO_AT_BRIDGE=1
  firefox -no-remote -profile /tmp/ff_fs_profile about:blank > /dev/null 2>&1
" &
FF_PID=$!

for s in {1..40}; do
    if grep -q "Toplevel mapped.*app_id='firefox'" "$COMP_LOG" 2>/dev/null; then
        echo "[+] Firefox mapped!"
        break
    fi
    sleep 0.3
done

sleep 2
echo "[5] Toggling fullscreen on Firefox..."
echo "fullscreen" > "$FIFO_PATH"
sleep 1.5

echo "[6] Checking FullscreenDebug logs..."
grep "FullscreenDebug" "$COMP_LOG" || echo "No FullscreenDebug logs found"

echo "[7] Taking screenshot after fullscreen (TopBar MUST be obscured!)..."
echo "screenshot /workspace/build/firefox_over_topbar.ppm" > "$FIFO_PATH"
sleep 1

kill $FF_PID $SHELL_PID $COMP_PID 2>/dev/null || true
echo "[SUCCESS] Done!"
