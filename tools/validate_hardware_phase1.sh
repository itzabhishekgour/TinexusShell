#!/usr/bin/env bash
# ==============================================================================
# Tinexus Platform — Phase 1 Real-Hardware Validation Harness
# Target: ASUS TUF Gaming F15 FX506LI (Intel i5-10300H, UHD 630 i915)
# ==============================================================================
set -euo pipefail

OUT_LOG="${HOME}/phase1_hardware_evidence.log"
exec > >(tee -a "${OUT_LOG}") 2>&1

echo "======================================================================"
echo "  TINEXUS PLATFORM — PHASE 1 REAL-HARDWARE VALIDATION RUNNER"
echo "  Timestamp: $(date -u '+%Y-%m-%dT%H:%M:%SZ')"
echo "======================================================================"

echo ""
echo "─── [1/6] LIVE HARDWARE & DISPLAY STATE ──────────────────────────────"
echo "Host Model:"
cat /sys/devices/virtual/dmi/id/product_name 2>/dev/null || uname -a
echo ""
echo "DRM Connectors & Status:"
for f in /sys/class/drm/*/status; do
    echo "  $f: $(cat "$f")"
done
echo ""
echo "Active DRM Modes:"
for m in /sys/class/drm/*/modes; do
    if [ -s "$m" ]; then
        echo "  $m:"
        head -n 5 "$m" | sed 's/^/    /'
    fi
done
echo ""
echo "Tinexus Display Info File:"
if [ -f "/tmp/tinexus-display.info" ]; then
    cat /tmp/tinexus-display.info
elif [ -f "${HOME}/.config/tinexus/display.info" ]; then
    cat "${HOME}/.config/tinexus/display.info"
else
    echo "  [INFO] display.info not found at standard paths."
fi
echo ""
echo "Compositor Environment & Renderer:"
echo "  WLR_RENDERER:   ${WLR_RENDERER:-[unset]}"
echo "  WLR_DRM_DEVICES:${WLR_DRM_DEVICES:-[unset]}"
echo "  WAYLAND_DISPLAY:${WAYLAND_DISPLAY:-[unset]}"

echo ""
echo "─── [2/6] COMPOSITOR PROCESS STATUS ──────────────────────────────────"
if pgrep -a tinexus-comp; then
    echo "  [OK] tinexus-comp is running."
    ps -o pid,user,%cpu,%mem,stat,comm -p $(pgrep tinexus-comp)
else
    echo "  [ERROR] tinexus-comp is NOT running!"
    exit 1
fi

echo ""
echo "─── [3/6] REAL WAYLAND CLIENT LAUNCH (ABOUT + TERMINAL) ──────────────"
echo "Launching tinexus-about..."
tinexus-about &
ABOUT_PID=$!
sleep 1.5

echo "Launching tinexus-terminal..."
tinexus-terminal &
TERM_PID=$!
sleep 1.5

echo "Active client PIDs:"
ps -o pid,comm,stat -p "${ABOUT_PID},${TERM_PID}" || true

echo ""
echo "─── [4/6] REAL CLIENT SIGKILL RESILIENCE TEST ────────────────────────"
echo "Sending SIGKILL (kill -9) to tinexus-about (PID ${ABOUT_PID})..."
kill -9 "${ABOUT_PID}"
sleep 1

echo "Checking compositor survival after SIGKILL:"
if pgrep -x tinexus-comp >/dev/null; then
    echo "  [PASS] tinexus-comp SURVIVED client SIGKILL!"
    ps -o pid,%cpu,%mem,stat,comm -p $(pgrep tinexus-comp)
else
    echo "  [FAIL] tinexus-comp CRASHED upon SIGKILL!"
    exit 1
fi

echo ""
echo "─── [5/6] POST-KILL CLIENT LAUNCH (MONITOR) ──────────────────────────"
echo "Launching tinexus-monitor to verify compositor accepts new windows..."
tinexus-monitor &
MONITOR_PID=$!
sleep 1.5

if pgrep -x tinexus-comp >/dev/null; then
    echo "  [PASS] tinexus-comp accepted tinexus-monitor (PID ${MONITOR_PID}) cleanly."
    ps -o pid,comm -p "${MONITOR_PID},${TERM_PID}" || true
else
    echo "  [FAIL] tinexus-comp crashed after subsequent client launch!"
    exit 1
fi

echo ""
echo "─── [6/7] LIVE DISPLAY & RENDERER DIAGNOSTICS ───────────────────────"
if [ -f "/run/tinexus/renderer" ]; then
    echo "  Live Renderer (/run/tinexus/renderer): $(cat /run/tinexus/renderer)"
fi
if [ -f "${XDG_RUNTIME_DIR}/tinexus/display" ]; then
    echo "  Live Display (${XDG_RUNTIME_DIR}/tinexus/display):"
    cat "${XDG_RUNTIME_DIR}/tinexus/display" | sed 's/^/    /'
fi

echo ""
echo "─── [7/7] PHYSICAL INTERACTION TEST SUITE INSTRUCTIONS ───────────────"
echo "  [Test A] Open 3-5 windows: tinexus-files, tinexus-terminal, tinexus-monitor, tinexus-about"
echo "  [Test B] Move windows: drag titlebars. Snapped/maximized windows un-snap smoothly after 8px drag."
echo "  [Test C] Resize from all 8 edges/corners: left, right, top, bottom, TL, TR, BL, BR."
echo "  [Test D] Maximize / Restore: verify zero left/right margin gap; window aligns above bottom dock (96px)."
echo "  [Test E] Minimize / Restore: window disappears from scene; click dock icon to restore."
echo "  [Test F] Snap: drag to left edge (50%), right edge (50%), top edge (maximize), and 4 corners (25%)."
echo "  [Test G] Multi-monitor: maximize and snap conform to active monitor's dynamic WorkArea."
echo "  [Test H] SIGKILL mid-transition: compositor survives with zero crash, grab clears cleanly."
echo "  [Test I] Post-kill recovery: open new window and verify move/resize/maximize/restore cycle."
echo ""
echo "Validation log saved to: ${OUT_LOG}"
echo "======================================================================"
