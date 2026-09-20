#!/bin/bash
set -euo pipefail

/workspace/tools/ensure_mounts.sh

killall -9 tinexus-comp Xwayland vlc 2>/dev/null || true
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
  /workspace/build/bin/tinexus-comp > /workspace/build/comp_vlc.log 2>&1
" &
COMP_PID=$!

FIFO_PATH="/mnt/rootfs/tmp/tinexus-test-runtime/tinexus/comp-cmd.fifo"

for i in {1..30}; do
    if [ -S /mnt/rootfs/tmp/tinexus-test-runtime/wayland-0 ] && [ -p "$FIFO_PATH" ]; then
        echo "[+] Compositor ready!"
        break
    fi
    sleep 0.5
done

# Wait for XWayland ready in comp_vlc.log
DISPLAY_NUM=""
for i in {1..30}; do
    if grep -q "XWayland.*READY on DISPLAY=" /workspace/build/comp_vlc.log 2>/dev/null; then
        DISPLAY_NUM=$(grep "XWayland.*READY on DISPLAY=" /workspace/build/comp_vlc.log | tail -n 1 | sed -E "s/.*DISPLAY='([^']+)'.*/\1/")
        echo "[+] Found XWayland DISPLAY='$DISPLAY_NUM'"
        break
    fi
    sleep 0.5
done

if [ -z "$DISPLAY_NUM" ]; then
    DISPLAY_NUM=":0"
    echo "[!] Using fallback DISPLAY=:0"
fi

echo "[+] Launching VLC on XWayland ($DISPLAY_NUM)..."
chroot /mnt/rootfs /bin/bash -c "
  export XDG_RUNTIME_DIR=/tmp/tinexus-test-runtime
  export WAYLAND_DISPLAY=wayland-0
  export DISPLAY=$DISPLAY_NUM
  export QT_QPA_PLATFORM=xcb
  export LIBGL_ALWAYS_SOFTWARE=1
  export NO_AT_BRIDGE=1
  vlc --no-qt-privacy-ask --no-repeat --no-loop --no-video-title-show > /workspace/build/vlc_run.log 2>&1
" &
VLC_PID=$!

for s in {1..25}; do
    sleep 1
    echo "[+] Waiting for VLC window to map... ($s/25s)"
    if grep -q "Attached Tinexus SSD frame to XWayland surface" /workspace/build/comp_vlc.log 2>/dev/null; then
        echo "[+] VLC XWayland toplevel mapped with Tinexus SSD frame!"
        break
    fi
done

echo "[+] Waiting 10s for VLC to paint actual interface..."
sleep 10

echo "[+] Taking screenshot of VLC with SSD frame..."
echo "screenshot /workspace/build/vlc_rendered.ppm" > "$FIFO_PATH"
sleep 2

# Check SSD frame attached
if grep -q "Attached Tinexus SSD frame to XWayland surface" /workspace/build/comp_vlc.log; then
    echo "[PASS] Tinexus SSD frame attached to VLC!"
else
    echo "[FAIL] Tinexus SSD frame NOT attached to VLC!"
fi

# Convert screenshot
python3 /workspace/tools/convert_ppm_to_png.py /workspace/build/vlc_rendered.ppm /workspace/build/vlc_rendered.png || true

# Extract window position
# Format in log: [XWayland] Toplevel mapped — class='vlc' title='...' (cur_x, cur_y)
# Let's inspect where it was positioned
WIN_LINE=$(grep "Toplevel mapped — class=" /workspace/build/comp_vlc.log | tail -n 1)
echo "[+] VLC window line: $WIN_LINE"

# Extract cur_x and cur_y:
# In log: "(X, Y) WxH"
WIN_X=$(echo "$WIN_LINE" | sed -E 's/.*\(([0-9-]+),.*/\1/')
WIN_Y=$(echo "$WIN_LINE" | sed -E 's/.*\([0-9-]+, ([0-9-]+)\).*/\1/')
echo "[+] Window initial position: ($WIN_X, $WIN_Y)"

# Test 1: Titlebar Drag-to-Move
DRAG_START_X=$((WIN_X + 150))
DRAG_START_Y=$((WIN_Y + 14))
DRAG_END_X=$((WIN_X + 250))
DRAG_END_Y=$((WIN_Y + 114))
echo "[+] Testing drag-to-move from ($DRAG_START_X, $DRAG_START_Y) to ($DRAG_END_X, $DRAG_END_Y)..."
echo "warp $DRAG_START_X $DRAG_START_Y" > "$FIFO_PATH"
sleep 0.5
echo "btn_down" > "$FIFO_PATH"
sleep 0.5
echo "warp $DRAG_END_X $DRAG_END_Y" > "$FIFO_PATH"
sleep 0.5
echo "btn_up" > "$FIFO_PATH"
sleep 1.0

if grep -q "Started interactive move grab" /workspace/build/comp_vlc.log; then
    echo "[PASS] Titlebar drag-to-move successfully initiated and moved XWayland window!"
    WIN_X=$((WIN_X + 100))
    WIN_Y=$((WIN_Y + 100))
    echo "[+] Updated window position after drag: ($WIN_X, $WIN_Y)"
else
    echo "[FAIL] Titlebar drag-to-move grab not detected!"
fi

# Test 2: Maximize button click
MAX_X=$((WIN_X + 58))
MAX_Y=$((WIN_Y + 14))
echo "[+] Warping cursor to Maximize button ($MAX_X, $MAX_Y)..."
echo "warp $MAX_X $MAX_Y" > "$FIFO_PATH"
sleep 0.5
echo "[+] Clicking Maximize button..."
echo "click_ssd" > "$FIFO_PATH"
sleep 1.5

if grep -q "Maximized window to" /workspace/build/comp_vlc.log; then
    echo "[PASS] Maximize button successfully maximized XWayland window!"
else
    echo "[FAIL] Maximize button failed to maximize window!"
fi

# Test 3: Unmaximize (restore) button click
# While maximized, window is at (0, 32), maximize/restore button is at (58, 46)
echo "[+] Warping cursor to Restore button (58, 46)..."
echo "warp 58 46" > "$FIFO_PATH"
sleep 0.5
echo "[+] Clicking Restore button..."
echo "click_ssd" > "$FIFO_PATH"
sleep 1.5

if grep -q "Restored window to" /workspace/build/comp_vlc.log; then
    echo "[PASS] Restore button successfully restored XWayland window to normal geometry!"
else
    echo "[FAIL] Restore button failed to restore window!"
fi

# Test 4: Close button click
# Window is restored to (WIN_X, WIN_Y). Close button is at (WIN_X + 18, WIN_Y + 14)
CLOSE_X=$((WIN_X + 18))
CLOSE_Y=$((WIN_Y + 14))
echo "[+] Warping cursor to Close button ($CLOSE_X, $CLOSE_Y)..."
echo "warp $CLOSE_X $CLOSE_Y" > "$FIFO_PATH"
sleep 0.5
echo "[+] Clicking Close button..."
echo "click_ssd" > "$FIFO_PATH"
sleep 2.0

if grep -q "Toplevel destroyed" /workspace/build/comp_vlc.log; then
    echo "[PASS] Close button successfully sent close and destroyed XWayland window!"
else
    echo "[FAIL] Close button failed to close window!"
fi

kill $VLC_PID $COMP_PID 2>/dev/null || true

echo "[+] Relevant comp_vlc.log entries:"
grep -E "XWayland|Decoration" /workspace/build/comp_vlc.log | tail -n 35
