#!/bin/bash
set -eo pipefail

mkdir -p /tmp/wlr_run /run/tinexus
chmod 700 /tmp/wlr_run
export XDG_RUNTIME_DIR=/tmp/wlr_run
export WAYLAND_DISPLAY=wayland-0
export WLR_BACKENDS=headless
export WLR_RENDERER=pixman
export WLR_LIBINPUT_NO_DEVICES=1

rm -f /tmp/x11_capture.png /tmp/comp_x11.log /tmp/x11_client.log /run/tinexus/env /workspace/x11_screencopy_verified.png

echo "[STEP 1] Starting headless tinexus-comp..."
/workspace/build/bin/tinexus-comp > /tmp/comp_x11.log 2>&1 &
COMP_PID=$!

cleanup() {
    echo "[CLEANUP] Stopping processes..."
    kill -9 $X11_PID 2>/dev/null || true
    kill -9 $COMP_PID 2>/dev/null || true
    rm -rf /tmp/wlr_run
}
trap cleanup EXIT

# Wait up to 10 seconds for tinexus-comp and Xwayland socket to initialize
echo "[STEP 2] Waiting for Wayland display socket and /run/tinexus/env..."
for i in $(seq 1 20); do
    if [ -S /tmp/wlr_run/wayland-0 ] && [ -f /run/tinexus/env ]; then
        echo "Compositor ready and /run/tinexus/env created in ${i}00ms!"
        break
    fi
    sleep 0.5
done

if [ ! -f /run/tinexus/env ]; then
    echo "[ERROR] /run/tinexus/env was not created!"
    cat /tmp/comp_x11.log
    exit 1
fi

echo "[STEP 3] Verifying /run/tinexus/env contents..."
cat /run/tinexus/env
# Source environment as Dock/Launcher runtime helper does
export $(cat /run/tinexus/env | xargs)
echo "Inherited DISPLAY: $DISPLAY"

if [ -z "$DISPLAY" ]; then
    echo "[ERROR] DISPLAY is empty!"
    exit 1
fi

echo "[STEP 4] Launching real X11 client (test_x11_client)..."
/workspace/tools/test_x11_client > /tmp/x11_client.log 2>&1 &
X11_PID=$!

echo "[STEP 5] Waiting for Xwayland client to map and render..."
MAPPED=0
for i in $(seq 1 20); do
    if grep -q "Toplevel mapped" /tmp/comp_x11.log 2>/dev/null; then
        echo "X11 Toplevel mapped in ${i}00ms!"
        MAPPED=1
        break
    fi
    sleep 0.5
done

if [ "$MAPPED" -eq 0 ]; then
    echo "[ERROR] X11 window was not mapped in time!"
    echo "=== Compositor Log ==="
    cat /tmp/comp_x11.log
    echo "=== X11 Client Log ==="
    cat /tmp/x11_client.log
    exit 1
fi

# Extra time for buffer commit and scene rendering
sleep 1

echo "[STEP 6] Compositor XWayland logs:"
grep -E "\[XWayland\]" /tmp/comp_x11.log || true

echo "[STEP 7] Capturing frame buffer via grim screencopy..."
grim /tmp/x11_capture.png
GRIM_STATUS=$?
echo "Grim exit code: $GRIM_STATUS"

if [ -f /tmp/x11_capture.png ]; then
    echo "[SUCCESS] X11 window captured successfully!"
    ls -lh /tmp/x11_capture.png
    file /tmp/x11_capture.png
    cp /tmp/x11_capture.png /workspace/x11_screencopy_verified.png
else
    echo "[ERROR] /tmp/x11_capture.png not found!"
    cat /tmp/comp_x11.log
    cat /tmp/x11_client.log
    exit 1
fi

echo "[ALL CHECKS PASSED] Real X11 client rendered and captured via XWayland!"
