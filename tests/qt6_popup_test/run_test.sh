#!/bin/bash
set -e

export XDG_RUNTIME_DIR="/run/user/$(id -u)"
mkdir -p "$XDG_RUNTIME_DIR"

echo "=== [Phase A Test] Launching tinexus-comp (headless + pixman) ==="
rm -f /tmp/tinexus_comp_phase_a.log /tmp/tinexus_qt6_client_phase_a.log

WLR_BACKENDS=headless WLR_RENDERER=pixman ./build/debug/bin/tinexus-comp > /tmp/tinexus_comp_phase_a.log 2>&1 &
COMP_PID=$!

cleanup() {
    echo "=== [Phase A Test] Terminating tinexus-comp (PID $COMP_PID) ==="
    kill -TERM "$COMP_PID" 2>/dev/null || true
    wait "$COMP_PID" 2>/dev/null || true
}
trap cleanup EXIT

echo "=== [Phase A Test] Waiting for Wayland socket to be ready ==="
SOCKET=""
for i in {1..50}; do
    if [ -f /tmp/tinexus_comp_phase_a.log ]; then
        FOUND=$(grep -o 'WAYLAND_DISPLAY=wayland-[0-9]*' /tmp/tinexus_comp_phase_a.log 2>/dev/null | cut -d= -f2 | head -n1 || true)
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
    cat /tmp/tinexus_comp_phase_a.log
    exit 1
fi

echo "=== [Phase A Test] Launching tinexus-qt6-popup-test with --auto-test on $SOCKET ==="
QT_QPA_PLATFORM=wayland WAYLAND_DISPLAY="$SOCKET" \
    TINEXUS_POPUP_TEST_QML="$(pwd)/tests/qt6_popup_test/main.qml" \
    ./build/debug/bin/tinexus-qt6-popup-test --auto-test > /tmp/tinexus_qt6_client_phase_a.log 2>&1
CLIENT_EXIT=$?

echo "Client exited with code: $CLIENT_EXIT"

echo ""
echo "=== [Phase A Test] Client Log Output ==="
cat /tmp/tinexus_qt6_client_phase_a.log

echo ""
echo "=== [Phase A Test] FULL UNFILTERED Compositor Log Output ==="
cat /tmp/tinexus_comp_phase_a.log

echo ""
echo "=== [Phase A Test] Completed successfully ==="
exit $CLIENT_EXIT
