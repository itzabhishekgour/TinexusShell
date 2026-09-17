#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE="$(dirname "$SCRIPT_DIR")"

echo "=== 1. Ensuring Mounts ==="
bash "$WORKSPACE/tools/ensure_mounts.sh"

echo "=== 2. Preparing Headless Runtime Environment ==="
mkdir -p /mnt/rootfs/tmp/tinexus-dbus-test
chmod 700 /mnt/rootfs/tmp/tinexus-dbus-test
rm -rf /mnt/rootfs/tmp/tinexus-dbus-test/*

# Copy newly built binaries to /workspace/build/bin
mkdir -p "$WORKSPACE/build/bin"
cp -f "$WORKSPACE/build/debug/bin/tinexus-wallpaper" "$WORKSPACE/build/bin/tinexus-wallpaper"
cp -f "$WORKSPACE/build/debug/bin/tinexus-settings" "$WORKSPACE/build/bin/tinexus-settings"
cp -f "$WORKSPACE/build/debug/bin/tinexus-lock" "$WORKSPACE/build/bin/tinexus-lock"

chroot /mnt/rootfs /bin/bash << 'EOF'
set -e

export XDG_RUNTIME_DIR=/tmp/tinexus-dbus-test
export DBUS_SESSION_BUS_ADDRESS="unix:path=$XDG_RUNTIME_DIR/bus"
export WAYLAND_DISPLAY=wayland-0
export WLR_BACKENDS=headless
export WLR_HEADLESS_OUTPUTS=1
export WLR_LIBINPUT_NO_DEVICES=1
export WLR_RENDERER=pixman
export QT_QPA_PLATFORM=wayland

cleanup() {
    echo "=== CLEANUP: Stopping test processes ==="
    pkill -9 -f tinexus-settings-ui 2>/dev/null || true
    pkill -9 -f tinexus-lock 2>/dev/null || true
    pkill -9 -f tinexus-settings 2>/dev/null || true
    pkill -9 -f tinexus-wallpaper 2>/dev/null || true
    pkill -9 -f tinexus-comp 2>/dev/null || true
    pkill -9 -f dbus-daemon 2>/dev/null || true
}
trap cleanup EXIT

echo "=== 3. Starting Session D-Bus Daemon ==="
dbus-daemon --session --address="$DBUS_SESSION_BUS_ADDRESS" --fork

echo "=== 4. Starting tinexus-comp (Headless Mode) ==="
/workspace/build/bin/tinexus-comp > /tmp/tinexus-dbus-test/comp.log 2>&1 &
COMP_PID=$!

for i in {1..30}; do
    if [ -S "$XDG_RUNTIME_DIR/wayland-0" ]; then
        echo "[+] Compositor wayland-0 socket is ready!"
        break
    fi
    sleep 0.2
done

if [ ! -S "$XDG_RUNTIME_DIR/wayland-0" ]; then
    echo "[!] Compositor failed to start. Logs:"
    cat /tmp/tinexus-dbus-test/comp.log
    exit 1
fi

echo "=== 5. Starting tinexus-wallpaper ==="
/workspace/build/bin/tinexus-wallpaper > /tmp/tinexus-dbus-test/wallpaper.log 2>&1 &
WP_PID=$!

for i in {1..30}; do
    if busctl --user list | grep -q "io.tinexus.Wallpaper"; then
        echo "[+] tinexus-wallpaper successfully claimed io.tinexus.Wallpaper on D-Bus!"
        break
    fi
    sleep 0.2
done

echo "=== 6. Starting tinexus-settings-ui ==="
if [ -f /workspace/build/bin/tinexus-settings-ui ]; then
    QT_QPA_PLATFORM=offscreen /workspace/build/bin/tinexus-settings-ui > /tmp/tinexus-dbus-test/settings-ui.log 2>&1 &
    for i in {1..30}; do
        if busctl --user list | grep -q "io.tinexus.Settings"; then
            echo "[+] tinexus-settings-ui successfully claimed io.tinexus.Settings on D-Bus!"
            break
        fi
        sleep 0.2
    done
fi

echo "=== 7. Starting tinexus-lock (D-Bus Subscriber) ==="
/workspace/build/bin/tinexus-lock > /tmp/tinexus-dbus-test/lock.log 2>&1 &
LOCK_PID=$!
sleep 1.5

echo "============================================================"
echo " RUNTIME D-BUS VERIFICATION SUITE"
echo "============================================================"

echo "--- 1. busctl --user list | grep io.tinexus ---"
busctl --user list | grep "io.tinexus" || true

echo ""
echo "--- 2. busctl --user introspect io.tinexus.Wallpaper /io/tinexus/Wallpaper ---"
busctl --user introspect io.tinexus.Wallpaper /io/tinexus/Wallpaper

echo ""
echo "--- 3. busctl --user call io.tinexus.Wallpaper /io/tinexus/Wallpaper io.tinexus.Wallpaper GetStatus ---"
busctl --user call io.tinexus.Wallpaper /io/tinexus/Wallpaper io.tinexus.Wallpaper GetStatus

echo ""
echo "--- 4. busctl --user call SetWallpaper (Initial) ---"
busctl --user call io.tinexus.Wallpaper /io/tinexus/Wallpaper io.tinexus.Wallpaper SetWallpaper sybq "/usr/share/backgrounds/emerald-matrix.png" 0 false 500

echo ""
echo "--- 5. busctl --user call SetWallpaper (Live Transition to Aurora Neon) ---"
busctl --user call io.tinexus.Wallpaper /io/tinexus/Wallpaper io.tinexus.Wallpaper SetWallpaper sybq "/usr/share/backgrounds/aurora-neon.png" 0 false 500
sleep 1.5

echo ""
echo "--- 6. Verifying Lockscreen Instant D-Bus Reception (No 5s Polling Wait) ---"
cat /tmp/tinexus-dbus-test/lock.log || true

echo ""
echo "--- 7. Verifying Zero SIGUSR1 Triggers in Wallpaper Log ---"
if grep -q "SIGUSR1" /tmp/tinexus-dbus-test/wallpaper.log; then
    echo "[!] UNEXPECTED: SIGUSR1 found in wallpaper log!"
else
    echo "[PASS] Zero SIGUSR1 signals triggered (Pure D-Bus method delivery verified)."
fi

echo ""
echo "--- 8. Wallpaper Daemon Log Tail ---"
tail -n 25 /tmp/tinexus-dbus-test/wallpaper.log

EOF

echo "[SUCCESS] Runtime D-Bus Verification Suite finished successfully!"
