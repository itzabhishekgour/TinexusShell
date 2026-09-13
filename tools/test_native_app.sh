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
  /workspace/build/bin/tinexus-comp > /workspace/build/comp_native.log 2>&1
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

echo "[+] Launching tinexus-monitor (Native Qt6 CSD App)..."
chroot /mnt/rootfs /bin/bash -c "
  export XDG_RUNTIME_DIR=/tmp/tinexus-test-runtime
  export WAYLAND_DISPLAY=wayland-0
  export QT_QPA_PLATFORM=wayland
  export QT_WAYLAND_DISABLE_WINDOWDECORATION=1
  /workspace/build/bin/tinexus-monitor > /workspace/build/monitor_test.log 2>&1
" &
APP_PID=$!

for s in {1..10}; do
    sleep 1
    echo "[+] Waiting for tinexus-monitor window... ($s/10s)"
    if grep -q "New XDG toplevel surface created" /workspace/build/comp_native.log; then
        echo "[+] tinexus-monitor toplevel surface DETECTED by tinexus-comp!"
        break
    fi
done

sleep 3

echo "[+] Taking screendump..."
echo "screenshot /workspace/build/native_rendered.ppm" > "$FIFO_PATH"
sleep 2

kill $APP_PID $COMP_PID 2>/dev/null || true

echo "[+] comp_native.log tail:"
tail -n 25 /workspace/build/comp_native.log

echo "[+] Converting native render to PNG..."
chroot /mnt/rootfs python3 /workspace/tools/ppm_to_png.py /workspace/build/native_rendered.ppm /workspace/build/native_csd_rendered.png || true

ARTIFACT_DIR="/mnt/c/Users/mrasg/.gemini/antigravity-ide/brain/11c1548d-72a5-4faa-99ca-acd0b1f4870b"
cp -f /workspace/build/native_csd_rendered.png "$ARTIFACT_DIR/native_csd_rendered.png" 2>/dev/null || true
echo "[+] Done!"
