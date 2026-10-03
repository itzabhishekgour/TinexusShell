#!/bin/bash
set -euo pipefail

echo "========================================================"
echo "    TINEXUS COMPOSITOR VISUAL RENDER VERIFICATION GATE   "
echo "========================================================"

# 1. Ensure mounts
/workspace/tools/ensure_mounts.sh

# 2. Setup runtime environment inside rootfs
mkdir -p /mnt/rootfs/tmp/tinexus-test-runtime
chmod 700 /mnt/rootfs/tmp/tinexus-test-runtime
rm -rf /mnt/rootfs/tmp/tinexus-test-runtime/*
rm -rf /mnt/rootfs/tmp/ff_test_profile
mkdir -p /mnt/rootfs/tmp/ff_test_profile
rm -f /workspace/build/foot_rendered.ppm /workspace/build/firefox_rendered.ppm
rm -f /workspace/build/foot_ssd_rendered.png /workspace/build/firefox_ssd_rendered.png

echo "[1/5] Starting tinexus-comp in headless test mode..."
chroot /mnt/rootfs /bin/bash -c "
  export XDG_RUNTIME_DIR=/tmp/tinexus-test-runtime
  export WAYLAND_DISPLAY=wayland-0
  export WLR_BACKENDS=headless
  export WLR_HEADLESS_OUTPUTS=1
  export WLR_LIBINPUT_NO_DEVICES=1
  export WLR_RENDERER=pixman
  /workspace/build/bin/tinexus-comp > /workspace/build/comp_test.log 2>&1
" &
COMP_PID=$!

FIFO_PATH="/mnt/rootfs/tmp/tinexus-test-runtime/tinexus/comp-cmd.fifo"

# Wait for wayland-0 and FIFO to be ready
echo "[+] Waiting for compositor socket and command FIFO..."
for i in {1..30}; do
    if [ -S /mnt/rootfs/tmp/tinexus-test-runtime/wayland-0 ] && [ -p "$FIFO_PATH" ]; then
        echo "[+] Compositor and FIFO are up and running! (iteration $i)"
        break
    fi
    sleep 0.5
done

if [ ! -S /mnt/rootfs/tmp/tinexus-test-runtime/wayland-0 ] || [ ! -p "$FIFO_PATH" ]; then
    echo "[!] ERROR: Compositor failed to start or FIFO not created!"
    cat /workspace/build/comp_test.log
    kill $COMP_PID 2>/dev/null || true
    exit 1
fi

echo "[2/5] Launching foot terminal under tinexus-comp..."
chroot /mnt/rootfs /bin/bash -c "
  export XDG_RUNTIME_DIR=/tmp/tinexus-test-runtime
  export WAYLAND_DISPLAY=wayland-0
  foot --title='Tinexus Terminal' /bin/sh -c 'echo \"=== TINEXUS SSD TERMINAL ===\"; echo \"Resolution: 1280x720\"; uname -a; date; exec /bin/sh'
" > /workspace/build/foot_test.log 2>&1 &
FOOT_PID=$!

echo "[+] Waiting for foot window to map and render..."
sleep 4

echo "[+] Triggering screendump for foot..."
echo "screenshot /workspace/build/foot_rendered.ppm" > "$FIFO_PATH"
sleep 2

if [ -f /workspace/build/foot_rendered.ppm ]; then
    echo "[SUCCESS] foot_rendered.ppm successfully captured ($(stat -c%s /workspace/build/foot_rendered.ppm) bytes)!"
else
    echo "[!] ERROR: foot_rendered.ppm was not created!"
    cat /workspace/build/comp_test.log
    cat /workspace/build/foot_test.log
    kill $FOOT_PID $COMP_PID 2>/dev/null || true
    exit 1
fi

# Close foot before launching firefox
kill $FOOT_PID 2>/dev/null || true
sleep 1

echo "[3/5] Launching Firefox under tinexus-comp..."
chroot /mnt/rootfs /bin/bash -c "
  export XDG_RUNTIME_DIR=/tmp/tinexus-test-runtime
  export WAYLAND_DISPLAY=wayland-0
  export MOZ_ENABLE_WAYLAND=1
  export LIBGL_ALWAYS_SOFTWARE=1
  firefox -no-remote -profile /tmp/ff_test_profile about:blank
" > /workspace/build/firefox_test.log 2>&1 &
FIREFOX_PID=$!

echo "[+] Waiting for Firefox to initialize and map surface (7s)..."
sleep 7

echo "[+] Triggering screendump for Firefox..."
echo "screenshot /workspace/build/firefox_rendered.ppm" > "$FIFO_PATH"
sleep 2

if [ -f /workspace/build/firefox_rendered.ppm ]; then
    echo "[SUCCESS] firefox_rendered.ppm successfully captured ($(stat -c%s /workspace/build/firefox_rendered.ppm) bytes)!"
else
    echo "[!] ERROR: firefox_rendered.ppm was not created!"
    cat /workspace/build/comp_test.log
    cat /workspace/build/firefox_test.log
    kill $FIREFOX_PID $COMP_PID 2>/dev/null || true
    exit 1
fi

echo "[4/5] Cleaning up test processes..."
kill $FIREFOX_PID $COMP_PID 2>/dev/null || true
sleep 1

echo "[5/5] Converting PPM captures to PNG..."
chroot /mnt/rootfs python3 /workspace/tools/ppm_to_png.py /workspace/build/foot_rendered.ppm /workspace/build/foot_ssd_rendered.png
chroot /mnt/rootfs python3 /workspace/tools/ppm_to_png.py /workspace/build/firefox_rendered.ppm /workspace/build/firefox_ssd_rendered.png

# Copy to artifact directory
ARTIFACT_DIR="/mnt/c/Users/mrasg/.gemini/antigravity-ide/brain/11c1548d-72a5-4faa-99ca-acd0b1f4870b"
mkdir -p "$ARTIFACT_DIR"
cp -f /workspace/build/foot_ssd_rendered.png "$ARTIFACT_DIR/foot_ssd_rendered.png"
cp -f /workspace/build/firefox_ssd_rendered.png "$ARTIFACT_DIR/firefox_ssd_rendered.png"

echo "========================================================"
echo "    VISUAL RENDER VERIFICATION COMPLETED SUCCESSFULLY   "
echo "========================================================"
ls -lh "$ARTIFACT_DIR/foot_ssd_rendered.png" "$ARTIFACT_DIR/firefox_ssd_rendered.png"
