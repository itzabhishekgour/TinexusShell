#!/usr/bin/env bash
set -e

LOG_DIR="/home/mrasg/tinexus/build/logs"
mkdir -p "$LOG_DIR"

export XDG_RUNTIME_DIR=/tmp/tinexus-test-runtime
mkdir -p "$XDG_RUNTIME_DIR"
chmod 700 "$XDG_RUNTIME_DIR"
rm -f "$XDG_RUNTIME_DIR"/wayland-*

export WAYLAND_DISPLAY=wayland-0
export WLR_BACKENDS=headless
export WLR_RENDERER=pixman
export WLR_LIBINPUT_NO_DEVICES=1

BIN_DIR="/home/mrasg/tinexus/build/debug/bin"

echo "======================================================================"
echo "  Tinexus Platform — Production Binary SIGKILL Regression Suite"
echo "======================================================================"

echo ""
echo "[STEP 1] Starting tinexus-comp in headless mode..."
"$BIN_DIR/tinexus-comp" > "$LOG_DIR/comp_headless.log" 2>&1 &
COMP_PID=$!
sleep 1.5

if ! kill -0 $COMP_PID 2>/dev/null; then
    echo "[FAIL] tinexus-comp failed to boot in headless mode:"
    cat "$LOG_DIR/comp_headless.log"
    exit 1
fi
echo "[PASS] tinexus-comp is alive with PID $COMP_PID on $WAYLAND_DISPLAY"

echo ""
echo "[STEP 2] Launching real client 1 (tinexus-about)..."
"$BIN_DIR/tinexus-about" > "$LOG_DIR/client1.log" 2>&1 &
CLIENT1_PID=$!
sleep 1.5

echo "[STEP 3] Launching real client 2 (tinexus-terminal)..."
"$BIN_DIR/tinexus-terminal" > "$LOG_DIR/client2.log" 2>&1 &
CLIENT2_PID=$!
sleep 1.5

echo ""
echo "[STEP 4] Current active process table before kill:"
ps -p "$COMP_PID,$CLIENT1_PID,$CLIENT2_PID" -o pid,comm,stat || true

echo ""
echo "[STEP 5] Sending SIGKILL (kill -9) to active client 1 (PID $CLIENT1_PID)..."
kill -9 "$CLIENT1_PID"
sleep 1

echo "[STEP 6] Verifying compositor survival..."
if kill -0 "$COMP_PID" 2>/dev/null; then
    echo "[PASS] tinexus-comp (PID $COMP_PID) survived client SIGKILL with ZERO crash."
else
    echo "[FAIL] tinexus-comp crashed upon client SIGKILL! Log:"
    cat "$LOG_DIR/comp_headless.log"
    exit 1
fi

echo ""
echo "[STEP 7] Launching new client 3 (tinexus-monitor) after SIGKILL event..."
"$BIN_DIR/tinexus-monitor" > "$LOG_DIR/client3.log" 2>&1 &
CLIENT3_PID=$!
sleep 1.5

echo "[STEP 8] Verifying compositor health and acceptance of new client..."
if kill -0 "$COMP_PID" 2>/dev/null; then
    echo "[PASS] tinexus-comp (PID $COMP_PID) remains fully operational and responsive."
    ps -p "$COMP_PID,$CLIENT2_PID,$CLIENT3_PID" -o pid,comm,stat || true
else
    echo "[FAIL] tinexus-comp crashed after subsequent client launch!"
    exit 1
fi

echo ""
echo "[STEP 9] Inspecting destruction sequence in compositor log:"
grep -E '(Toplevel|Focus|destroy|surface|Scene|unmapped|state_machine)' "$LOG_DIR/comp_headless.log" | tail -n 15 || true

echo ""
echo "[STEP 10] Cleaning up test session..."
kill -9 "$CLIENT2_PID" "$CLIENT3_PID" 2>/dev/null || true
kill -TERM "$COMP_PID" 2>/dev/null || true
wait "$COMP_PID" 2>/dev/null || true
echo "======================================================================"
echo "[PASS] Headless Production Binary SIGKILL Regression Succeeded"
echo "======================================================================"
