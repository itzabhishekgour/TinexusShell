#!/bin/bash
set -euo pipefail

/workspace/tools/ensure_mounts.sh

killall -9 tinexus-comp Xwayland xeyes 2>/dev/null || true
rm -rf /mnt/rootfs/tmp/tinexus-test-runtime/*
mkdir -p /mnt/rootfs/tmp/tinexus-test-runtime
chmod 700 /mnt/rootfs/tmp/tinexus-test-runtime
rm -rf /mnt/rootfs/tmp/.X11-unix/*
mkdir -p /mnt/rootfs/tmp/.X11-unix
chmod 1777 /mnt/rootfs/tmp/.X11-unix

echo "[+] Starting tinexus-comp..."
chroot /mnt/rootfs /bin/bash -c "
  export XDG_RUNTIME_DIR=/tmp/tinexus-test-runtime
  export WAYLAND_DISPLAY=wayland-0
  export WLR_BACKENDS=headless
  export WLR_HEADLESS_OUTPUTS=1
  export WLR_LIBINPUT_NO_DEVICES=1
  export WLR_RENDERER=pixman
  /workspace/build/bin/tinexus-comp > /workspace/build/comp_xeyes.log 2>&1
" &
COMP_PID=$!

FIFO_PATH="/mnt/rootfs/tmp/tinexus-test-runtime/tinexus/comp-cmd.fifo"

for i in {1..20}; do
    if [ -S /mnt/rootfs/tmp/tinexus-test-runtime/wayland-0 ] && [ -p "$FIFO_PATH" ]; then
        echo "[+] Compositor ready!"
        break
    fi
    sleep 0.5
done

echo "[+] Launching xeyes on DISPLAY=:0..."
chroot /mnt/rootfs /bin/bash -c "
  export DISPLAY=:0
  xeyes > /workspace/build/xeyes_run.log 2>&1
" &
XEYES_PID=$!

for s in {1..15}; do
    sleep 1
    if grep -q "Attached Tinexus SSD frame to XWayland surface" /workspace/build/comp_xeyes.log 2>/dev/null; then
        echo "[+] xeyes XWayland toplevel mapped with Tinexus SSD frame!"
        break
    fi
done

sleep 2
echo "screenshot /workspace/build/xeyes_rendered.ppm" > "$FIFO_PATH"
sleep 1.5

kill $XEYES_PID $COMP_PID 2>/dev/null || true

if grep -q "Attached Tinexus SSD frame to XWayland surface" /workspace/build/comp_xeyes.log; then
    echo "[PASS] Tinexus SSD frame attached to xeyes!"
else
    echo "[FAIL] Tinexus SSD frame not attached to xeyes!"
fi

grep -E "XWayland|Decoration" /workspace/build/comp_xeyes.log | tail -n 20
