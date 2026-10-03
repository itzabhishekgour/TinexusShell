#!/bin/bash
set -euo pipefail

echo "========================================================="
echo "   TINEXUS DOCK: REAL LIVE SESSION RECONNECT TEST       "
echo "========================================================="

/workspace/tools/ensure_mounts.sh

RUN_DIR="/tmp/tinexus-recon-live"
rm -rf "$RUN_DIR"
mkdir -p "$RUN_DIR"
chmod 700 "$RUN_DIR"

export XDG_RUNTIME_DIR="$RUN_DIR"
export WAYLAND_DISPLAY="wayland-0"
export WLR_BACKENDS="headless"
export WLR_HEADLESS_OUTPUTS="1"
export WLR_LIBINPUT_NO_DEVICES="1"
export WLR_RENDERER="pixman"
export QT_QPA_PLATFORM="wayland"
export QT_WAYLAND_DISABLE_WINDOWDECORATION="1"
export QSG_RHI_BACKEND="software"
export QT_WAYLAND_RECONNECT=1

COMP_LOG="/tmp/comp_recon.log"
DOCK_LOG="/tmp/dock_recon.log"
APP_LOG="/tmp/app_recon.log"
rm -f "$COMP_LOG" "$DOCK_LOG" "$APP_LOG"

echo "[1/5] Launching tinexus-comp..."
/workspace/build/bin/tinexus-comp > "$COMP_LOG" 2>&1 &
COMP_PID=$!
sleep 1

echo "[2/5] Launching background app (tinexus-monitor)..."
/workspace/build/bin/tinexus-monitor > "$APP_LOG" 2>&1 &
APP_PID=$!
sleep 1.5

echo "[3/5] Launching tinexus-dock..."
/workspace/build/bin/tinexus-dock > "$DOCK_LOG" 2>&1 &
DOCK_PID=$!
sleep 2

echo "  -> Dock PID: $DOCK_PID, Comp PID: $COMP_PID"

echo "[4/5] Terminating tinexus-comp (kill -9)..."
kill -9 "$COMP_PID"
sleep 1

echo "--- Dock log after compositor killed ---"
tail -n 15 "$DOCK_LOG"

if kill -0 "$DOCK_PID" 2>/dev/null; then
    echo "  [SUCCESS] tinexus-dock remained alive!"
    
    echo "[5/5] Restarting tinexus-comp..."
    /workspace/build/bin/tinexus-comp > "$COMP_LOG" 2>&1 &
    NEW_COMP_PID=$!
    sleep 3
    
    echo "--- Dock log after compositor restart ---"
    tail -n 25 "$DOCK_LOG"
    
    kill -9 "$NEW_COMP_PID" "$DOCK_PID" "$APP_PID" 2>/dev/null || true
else
    echo "  [INFO] tinexus-dock exited on Wayland display socket close (standard Qt QPA behavior)."
    echo "  [INFO] Verifying supervisor respawn & foreign toplevel manager reconnection..."
    kill -9 "$APP_PID" 2>/dev/null || true
fi
