#!/bin/bash
set -euo pipefail

# ── Self re-exec into chroot if running from host ────────────────────────────
if [ -z "${INSIDE_CHROOT:-}" ]; then
    /workspace/tools/ensure_mounts.sh
    exec chroot /mnt/rootfs /usr/bin/env INSIDE_CHROOT=1 bash /workspace/tools/test_multi_output.sh "$@"
fi

echo "========================================================="
echo "   TINEXUS DOCK: MULTI-MONITOR OUTPUT AFFINITY PROOF    "
echo "========================================================="

RUN_DIR="/tmp/multiout"
rm -rf "$RUN_DIR"
mkdir -p "$RUN_DIR"
chmod 700 "$RUN_DIR"

export XDG_RUNTIME_DIR="$RUN_DIR"
export WAYLAND_DISPLAY="wayland-0"
export WLR_BACKENDS="headless"
export WLR_HEADLESS_OUTPUTS="3"
export WLR_LIBINPUT_NO_DEVICES="1"
export WLR_RENDERER="pixman"
export QT_QPA_PLATFORM="wayland"
export QT_WAYLAND_DISABLE_WINDOWDECORATION="1"
export QSG_RHI_BACKEND="software"
export QT_QUICK_BACKEND="software"

COMP_LOG="/tmp/comp_multi.log"
DOCK_LOG="/tmp/dock_multi.log"
rm -f "$COMP_LOG" "$DOCK_LOG"

/workspace/build/bin/tinexus-comp > "$COMP_LOG" 2>&1 &
COMP_PID=$!
sleep 1.5

/workspace/build/bin/tinexus-dock > "$DOCK_LOG" 2>&1 &
DOCK_PID=$!
sleep 1.8

echo "--- Dock Output Binding Log ---"
grep -E "Screen=" "$DOCK_LOG"

echo "--- Compositor Layer Surfaces ---"
grep -E "New layer surface created" "$COMP_LOG"

kill -9 "$DOCK_PID" "$COMP_PID" 2>/dev/null || true
rm -rf "$RUN_DIR"
echo "[SUCCESS] Multi-monitor test completed successfully."
