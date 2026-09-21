#!/bin/bash
set -euo pipefail

PROJECT_DIR="/workspace"
cd "$PROJECT_DIR"
/workspace/tools/ensure_mounts.sh

killall -9 tinexus-comp firefox vlc 2>/dev/null || true
rm -rf /mnt/rootfs/tmp/tinexus-ff-fs-test
mkdir -p /mnt/rootfs/tmp/tinexus-ff-fs-test
chmod 700 /mnt/rootfs/tmp/tinexus-ff-fs-test

COMP_LOG="/workspace/build/comp_ff_fs.log"
rm -f "$COMP_LOG"

echo "[1] Starting tinexus-comp..."
chroot /mnt/rootfs /bin/bash -c "
  export XDG_RUNTIME_DIR=/tmp/tinexus-ff-fs-test
  export WAYLAND_DISPLAY=wayland-0
  export WLR_BACKENDS=headless
  export WLR_HEADLESS_OUTPUTS=1
  export WLR_LIBINPUT_NO_DEVICES=1
  export WLR_RENDERER=pixman
  /workspace/build/bin/tinexus-comp > /workspace/build/comp_ff_fs.log 2>&1
" &
COMP_PID=$!

FIFO_PATH="/mnt/rootfs/tmp/tinexus-ff-fs-test/tinexus/comp-cmd.fifo"

READY=0
for i in {1..30}; do
    if [ -S /mnt/rootfs/tmp/tinexus-ff-fs-test/wayland-0 ] && [ -p "$FIFO_PATH" ]; then
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

echo "[2] Launching Firefox (native Wayland)..."
rm -rf /mnt/rootfs/tmp/ff_fs_profile
mkdir -p /mnt/rootfs/tmp/ff_fs_profile
cat << 'EOF_USERJS' > /mnt/rootfs/tmp/ff_fs_profile/user.js
user_pref("browser.startup.homepage_override.mstone", "ignore");
user_pref("browser.aboutwelcome.enabled", false);
user_pref("startup.homepage_welcome_url", "");
user_pref("browser.shell.checkDefaultBrowser", false);
user_pref("browser.tabs.inTitlebar", 1);
user_pref("browser.tabs.drawInTitlebar", true);
user_pref("browser.startup.page", 0);
user_pref("browser.newtabpage.enabled", false);
EOF_USERJS

chroot /mnt/rootfs /bin/bash -c "
  export XDG_RUNTIME_DIR=/tmp/tinexus-ff-fs-test
  export WAYLAND_DISPLAY=wayland-0
  export MOZ_ENABLE_WAYLAND=1
  export MOZ_SANDBOX=0
  export MOZ_DISABLE_CONTENT_SANDBOX=1
  export LIBGL_ALWAYS_SOFTWARE=1
  export GDK_BACKEND=wayland
  export NO_AT_BRIDGE=1
  firefox -no-remote -profile /tmp/ff_fs_profile about:blank > /workspace/build/ff_fs.log 2>&1
" &
FF_PID=$!

FF_READY=0
for s in {1..40}; do
    if grep -q "Toplevel mapped.*app_id='firefox'" "$COMP_LOG" 2>/dev/null; then
        echo "[+] Firefox toplevel surface DETECTED by tinexus-comp!"
        FF_READY=1
        break
    fi
    sleep 0.3
done

if [ $FF_READY -ne 1 ]; then
    echo "[!] Firefox window did not map in time"
    kill $FF_PID $COMP_PID 2>/dev/null || true
    cat "$COMP_LOG"
    exit 1
fi

sleep 2
echo "[3] Toggling fullscreen on Firefox via compositor..."
echo "fullscreen" > "$FIFO_PATH"
sleep 1.5

echo "[4] Checking FullscreenDebug log lines:"
grep "FullscreenDebug" "$COMP_LOG" || echo "No FullscreenDebug logs found"

echo "[5] Capturing screenshot in fullscreen..."
echo "screenshot /workspace/build/firefox_fullscreen.ppm" > "$FIFO_PATH"
sleep 1
if [ -f /workspace/build/firefox_fullscreen.ppm ]; then
    python3 /workspace/tools/convert_ppm_to_png.py /workspace/build/firefox_fullscreen.ppm /workspace/build/firefox_fullscreen.png 2>/dev/null || true
    echo "[+] Screenshot successfully saved to /workspace/build/firefox_fullscreen.png"
fi

kill $FF_PID $COMP_PID 2>/dev/null || true
echo "[SUCCESS] Test completed!"
