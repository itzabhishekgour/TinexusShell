#!/bin/bash
set -e

export XDG_RUNTIME_DIR="/run/user/$(id -u)"
mkdir -p "$XDG_RUNTIME_DIR"

echo "=== [Phase B Test] Launching tinexus-comp (headless + pixman) ==="
rm -f /tmp/tinexus_comp_phase_b.log /tmp/tinexus_qt6_settings_phase_b.log

WLR_BACKENDS=headless WLR_RENDERER=pixman ./build/debug/bin/tinexus-comp > /tmp/tinexus_comp_phase_b.log 2>&1 &
COMP_PID=$!

cleanup() {
    echo "=== [Phase B Test] Terminating tinexus-comp (PID $COMP_PID) ==="
    kill -TERM "$COMP_PID" 2>/dev/null || true
    wait "$COMP_PID" 2>/dev/null || true
}
trap cleanup EXIT

echo "=== [Phase B Test] Waiting for Wayland socket to be ready ==="
SOCKET=""
for i in {1..50}; do
    if [ -f /tmp/tinexus_comp_phase_b.log ]; then
        FOUND=$(grep -o 'WAYLAND_DISPLAY=wayland-[0-9]*' /tmp/tinexus_comp_phase_b.log 2>/dev/null | cut -d= -f2 | head -n1 || true)
        if [ -n "$FOUND" ] && [ -S "$XDG_RUNTIME_DIR/$FOUND" ]; then
            SOCKET="$FOUND"
            echo "Compositor announced and bound socket: $SOCKET"
            break
        fi
    fi
    sleep 0.1
done

if [ -z "$SOCKET" ]; then
    echo "ERROR: Wayland socket did not appear within 5 seconds!"
    cat /tmp/tinexus_comp_phase_b.log
    exit 1
fi

echo "=== [Phase B Test] Launching Qt6 tinexus-settings-ui with --auto-test on $SOCKET ==="
QT_QPA_PLATFORM=wayland WAYLAND_DISPLAY="$SOCKET" \
    TINEXUS_SETTINGS_QML="$(pwd)/src/settings-ui/qml/MainWindow.qml" \
    ./build/debug/bin/tinexus-settings-ui --auto-test > /tmp/tinexus_qt6_settings_phase_b.log 2>&1
CLIENT_EXIT=$?

echo "Client exited with code: $CLIENT_EXIT"

echo ""
echo "=== [Phase B Test] Client Log Output ==="
cat /tmp/tinexus_qt6_settings_phase_b.log

echo ""
echo "=== [Phase B Test] FULL UNFILTERED Compositor Log Output ==="
cat /tmp/tinexus_comp_phase_b.log

echo ""
echo "=== [Phase B Test] Completed successfully with code $CLIENT_EXIT ==="
exit $CLIENT_EXIT
