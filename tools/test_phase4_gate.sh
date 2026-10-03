#!/usr/bin/env bash
# ============================================================================
# test_phase4_gate.sh — Phase 4 Gate: Multi-Tab Navigation Verification
#
# Builds test-phase4-tabs in rootfs chroot and executes all 5 verification checks:
#   TAB-1: Initial State & Tab Invariants
#   TAB-2: Create Tab (Ctrl+T) & Independent Path Isolation
#   TAB-3: Independent History, View Mode, and Sort State Per Tab
#   TAB-4: Close Tab (Ctrl+W) & Seamless Focus Transition
#   TAB-5: Bounds Checking & Rapid Switching
# ============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE="$(dirname "$SCRIPT_DIR")"
LOG_FILE="${WORKSPACE}/build/phase4_gate.log"
ARTIFACTS_DIR="/mnt/host/c/Users/mrasg/.gemini/antigravity-ide/brain/d0d1fb08-bc15-4e60-b76f-51e2393134ef"

mkdir -p "${WORKSPACE}/build"

echo "======================================================================"
echo "  Tinexus Phase 4 Gate: Multi-Tab Navigation Automated Gate"
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
chroot /mnt/rootfs /usr/bin/env PATH=/usr/bin:/bin ls /workspace/src/files/test_phase4_tabs.cpp 2>&1 && echo "[+] test_phase4_tabs.cpp visible in chroot"

# ── 3. Build test-phase4-tabs target ──────────────────────────────────────
echo ""
echo "[3/4] Building test-phase4-tabs and tinexus-files..."
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
    cmake --build build/debug --target test-phase4-tabs tinexus-files -j\$(nproc) 2>&1
    echo '[+] Build SUCCESS'
    ls -lh /workspace/build/debug/bin/test-phase4-tabs /workspace/build/debug/bin/tinexus-files
" 2>&1 | tee "${LOG_FILE}"

# ── 4. Run Phase 4 Verification Suite ───────────────────────────────────────
echo ""
echo "[4/4] Executing Phase 4 automated test suite in chroot..."
chroot /mnt/rootfs /usr/bin/env PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin /usr/bin/bash -c "
    set -euo pipefail
    export QT_QPA_PLATFORM=offscreen
    export XDG_DATA_HOME=/root/.local/share
    export HOME=/root
    /workspace/build/debug/bin/test-phase4-tabs
" 2>&1 | tee -a "${LOG_FILE}"

echo ""
echo "======================================================================"
echo "  [ALL PHASE 4 CHECKS PASSED] Log written to: ${LOG_FILE}"
echo "======================================================================"

# Copy log to artifacts if accessible
if [ -d "${ARTIFACTS_DIR}" ]; then
    cp -f "${LOG_FILE}" "${ARTIFACTS_DIR}/phase4_gate.log" 2>/dev/null || true
    echo "[+] Log copied to artifacts directory: phase4_gate.log"
fi
