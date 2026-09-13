#!/bin/bash
set -euo pipefail

/workspace/tools/ensure_mounts.sh

mkdir -p /mnt/rootfs/tmp/tinexus-test-runtime
chmod 700 /mnt/rootfs/tmp/tinexus-test-runtime
rm -rf /mnt/rootfs/tmp/tinexus-test-runtime/*
rm -rf /mnt/rootfs/tmp/ff_test_profile
mkdir -p /mnt/rootfs/tmp/ff_test_profile

echo "[+] Starting tinexus-comp..."
chroot /mnt/rootfs /bin/bash -c "
  export XDG_RUNTIME_DIR=/tmp/tinexus-test-runtime
  export WAYLAND_DISPLAY=wayland-0
  export WLR_BACKENDS=headless
  export WLR_HEADLESS_OUTPUTS=1
  export WLR_LIBINPUT_NO_DEVICES=1
  export WLR_RENDERER=pixman
  /workspace/build/bin/tinexus-comp > /workspace/build/comp_ff.log 2>&1
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

echo "[+] Launching Firefox..."
chroot /mnt/rootfs /bin/bash -c "
  export XDG_RUNTIME_DIR=/tmp/tinexus-test-runtime
  export WAYLAND_DISPLAY=wayland-0
  export MOZ_ENABLE_WAYLAND=1
  export MOZ_SANDBOX=0
  export MOZ_DISABLE_CONTENT_SANDBOX=1
  export LIBGL_ALWAYS_SOFTWARE=1
  export GDK_BACKEND=wayland
  export NO_AT_BRIDGE=1
  firefox -no-remote -CreateProfile 'tinexus /tmp/ff_test_profile' 2>/dev/null || true
  firefox -no-remote -profile /tmp/ff_test_profile -new-window 'about:blank' > /workspace/build/ff_run.log 2>&1
" &
FF_PID=$!

for s in {1..15}; do
    sleep 1
    echo "[+] Waiting for Firefox window... ($s/15s)"
    if grep -q "New XDG toplevel surface created" /workspace/build/comp_ff.log; then
        echo "[+] Firefox toplevel surface DETECTED by tinexus-comp!"
        break
    fi
done

echo "[+] Waiting 6s for Firefox to paint client contents..."
sleep 6

echo "[+] Taking screendump..."
echo "screenshot /workspace/build/firefox_rendered.ppm" > "$FIFO_PATH"
sleep 2

kill $FF_PID $COMP_PID 2>/dev/null || true

echo "[+] comp_ff.log tail:"
tail -n 25 /workspace/build/comp_ff.log
echo "[+] ff_run.log tail:"
cat /workspace/build/ff_run.log 2>/dev/null || true
