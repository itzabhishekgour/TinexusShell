#!/bin/bash
set -euo pipefail

echo "=== Running Tinexus Session Lock Security Test ==="

PROJECT_DIR="/workspace"
cd "$PROJECT_DIR"
/workspace/tools/ensure_mounts.sh

killall -9 tinexus-comp 2>/dev/null || true
sleep 1

TEST_RUNTIME_DIR="/mnt/rootfs/tmp/tinexus-lock-test"
rm -rf "$TEST_RUNTIME_DIR"
mkdir -p "$TEST_RUNTIME_DIR"
chmod 700 "$TEST_RUNTIME_DIR"

COMP_LOG="/workspace/build/comp_lock_test.log"
rm -f "$COMP_LOG"

echo "[1/3] Starting tinexus-comp in headless mode..."
chroot /mnt/rootfs /bin/bash -c "
  export XDG_RUNTIME_DIR=/tmp/tinexus-lock-test
  export WAYLAND_DISPLAY=wayland-0
  export WLR_BACKENDS=headless
  export WLR_HEADLESS_OUTPUTS=1
  export WLR_LIBINPUT_NO_DEVICES=1
  export WLR_RENDERER=pixman
  /workspace/build/bin/tinexus-comp > /workspace/build/comp_lock_test.log 2>&1
" &
COMP_PID=$!

echo "[2/3] Waiting for Wayland socket..."
READY=0
for i in {1..30}; do
    if [ -S "$TEST_RUNTIME_DIR/wayland-0" ]; then
        READY=1
        break
    fi
    sleep 0.2
done

if [ $READY -ne 1 ]; then
    echo "[FATAL] tinexus-comp failed to initialize Wayland socket!"
    kill -9 $COMP_PID 2>/dev/null || true
    cat "$COMP_LOG"
    exit 1
fi
echo "[+] Compositor is up (PID: $COMP_PID, Socket: $TEST_RUNTIME_DIR/wayland-0)"

echo "[3/3] Executing security verification client..."
EXIT_CODE=0
chroot /mnt/rootfs /bin/bash -c "
  export XDG_RUNTIME_DIR=/tmp/tinexus-lock-test
  export WAYLAND_DISPLAY=wayland-0
  /workspace/tools/test_session_lock_security
" || EXIT_CODE=$?

echo ""
echo "Killing test compositor..."
kill -9 $COMP_PID 2>/dev/null || true
wait $COMP_PID 2>/dev/null || true

if [ $EXIT_CODE -eq 0 ]; then
    echo ">>> TEST SUCCESS: F-17 SECURITY VERIFICATION PASSED <<<"
else
    echo ">>> TEST FAILED with exit code $EXIT_CODE <<<"
    echo "--- Compositor Log Snippet ---"
    tail -n 30 "$COMP_LOG"
fi

exit $EXIT_CODE
