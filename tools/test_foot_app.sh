#!/bin/bash
set -euo pipefail

/workspace/tools/ensure_mounts.sh

mkdir -p /mnt/rootfs/tmp/tinexus-test-runtime
chmod 700 /mnt/rootfs/tmp/tinexus-test-runtime
rm -rf /mnt/rootfs/tmp/tinexus-test-runtime/*

echo "[+] Starting tinexus-comp..."
chroot /mnt/rootfs /bin/bash -c "
  export XDG_RUNTIME_DIR=/tmp/tinexus-test-runtime
  export WAYLAND_DISPLAY=wayland-0
  export WLR_BACKENDS=headless
  export WLR_HEADLESS_OUTPUTS=1
  export WLR_LIBINPUT_NO_DEVICES=1
  export WLR_RENDERER=pixman
  /workspace/build/bin/tinexus-comp > /workspace/build/comp_foot.log 2>&1
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

echo "[+] Launching foot terminal under tinexus-comp..."
chroot /mnt/rootfs /bin/bash -c "
  export XDG_RUNTIME_DIR=/tmp/tinexus-test-runtime
  export WAYLAND_DISPLAY=wayland-0
  foot --title='Tinexus Terminal' /bin/sh -c 'echo \"=== TINEXUS SSD TERMINAL ===\"; echo \"Resolution: 1280x720\"; uname -a; date; exec /bin/sh'
" > /workspace/build/foot_test.log 2>&1 &
FOOT_PID=$!

for s in {1..10}; do
    sleep 1
    echo "[+] Waiting for foot window... ($s/10s)"
    if grep -q "New XDG toplevel surface created" /workspace/build/comp_foot.log; then
        echo "[+] foot toplevel surface DETECTED by tinexus-comp!"
        break
    fi
done

sleep 3

echo "[+] Taking screendump..."
echo "screenshot /workspace/build/foot_rendered.ppm" > "$FIFO_PATH"
sleep 2

kill $FOOT_PID $COMP_PID 2>/dev/null || true

echo "[+] comp_foot.log tail:"
tail -n 25 /workspace/build/comp_foot.log
