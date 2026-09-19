#!/usr/bin/env bash
# ============================================================================
# test_phase7_gate.sh — Phase 7 Gate: Keyboard Shortcuts Verification
#
# Builds test-phase7-shortcuts in rootfs chroot and executes all 5 verification checks:
#   KEY-1: Selection Shortcuts (Ctrl+A -> selectAll, clearSelection)
#   KEY-2: Clipboard Shortcuts (Ctrl+C -> copySelected, Ctrl+X -> cutSelected, Ctrl+V -> pasteItem)
#   KEY-3: Deletion Shortcut (Delete -> deleteSelected)
#   KEY-4: Navigation Shortcuts (Backspace/Alt+Up -> goUp, Alt+Left/Right -> goBack/goForward)
#   KEY-5: Single-Item Guard Shortcuts (F2 / Enter invariant: selectedCount == 1)
# ============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE="$(dirname "$SCRIPT_DIR")"
LOG_FILE="${WORKSPACE}/build/phase7_gate.log"
ARTIFACTS_DIR="/mnt/host/c/Users/mrasg/.gemini/antigravity-ide/brain/d0d1fb08-bc15-4e60-b76f-51e2393134ef"

mkdir -p "${WORKSPACE}/build"

echo "======================================================================"
echo "  Tinexus Phase 7 Gate: Keyboard Shortcuts Automated Gate"
echo "  $(date -u '+%Y-%m-%dT%H:%M:%SZ')"
echo "======================================================================"

# ── 1. Ensure mounts ────────────────────────────────────────────────────────
echo ""
echo "[1/4] Ensuring rootfs mounts..."
bash "${WORKSPACE}/tools/ensure_mounts.sh"
echo "[+] Mounts OK"

# ── 2. Verify source ────────────────────────────────────────────────────────
echo ""
echo "[2/4] Verifying source files in chroot..."
chroot /mnt/rootfs /usr/bin/env PATH=/usr/bin:/bin ls /workspace/src/files/test_phase7_shortcuts.cpp 2>&1 && echo "[+] test_phase7_shortcuts.cpp visible in chroot"

# ── 3. Build test-phase7-shortcuts target ───────────────────────────────────
echo ""
echo "[3/4] Building test-phase7-shortcuts and tinexus-files..."
chroot /mnt/rootfs /usr/bin/env PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin /usr/bin/bash -c "
    set -euo pipefail
    cd /workspace
    cmake \
        -DENABLE_LEGACY_IPCD=OFF \
        -DBUILD_TESTING=OFF \
        -DTINEXUS_ENABLE_SANITIZERS=OFF \
        -B build/debug \
        -S . \
        2>&1 | tail -5
    rm -f /workspace/build/debug/bin/test-phase7-shortcuts /workspace/build/debug/bin/tinexus-files
    cmake --build build/debug --target test-phase7-shortcuts tinexus-files -j\$(nproc) 2>&1
    echo '[+] Build SUCCESS'
    ls -lh /workspace/build/debug/bin/test-phase7-shortcuts /workspace/build/debug/bin/tinexus-files
" 2>&1 | tee "${LOG_FILE}"

# ── 4. Run Phase 7 Verification Suite ───────────────────────────────────────
echo ""
echo "[4/4] Executing Phase 7 automated test suite in chroot..."
chroot /mnt/rootfs /usr/bin/env PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin /usr/bin/bash -c "
    set -euo pipefail
    export QT_QPA_PLATFORM=offscreen
    export XDG_DATA_HOME=/root/.local/share
    export HOME=/root
    /workspace/build/debug/bin/test-phase7-shortcuts
" 2>&1 | tee -a "${LOG_FILE}"

echo ""
echo "======================================================================"
echo "  [ALL PHASE 7 CHECKS PASSED] Log written to: ${LOG_FILE}"
echo "======================================================================"

if [ -d "${ARTIFACTS_DIR}" ]; then
    cp -f "${LOG_FILE}" "${ARTIFACTS_DIR}/phase7_gate.log" 2>/dev/null || true
    echo "[+] Log copied to artifacts directory: phase7_gate.log"
fi
