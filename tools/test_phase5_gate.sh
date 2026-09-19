#!/usr/bin/env bash
# ============================================================================
# test_phase5_gate.sh — Phase 5 Gate: Multi-Select & Batch Operations Verification
#
# Builds test-phase5-multiselect in rootfs chroot and executes all 6 verification checks:
#   MSEL-1: Single Item Selection & Backward-Compatible selectedPath
#   MSEL-2: Multi-Item Selection Primitives (toggle, range, selectAll, clear)
#   MSEL-3: Per-Tab Selection Isolation (Phase 4 Tab Model integration)
#   MSEL-4: Batch Operations (Batch Copy, Cut, Delete/Trash, and Tagging)
#   MSEL-5: Batch Conflict "Apply to All" Persistence Across N Items
#   MSEL-6: Edge Cases: Mixed File+Folder Batch, Mid-Selection Delete, Stale Index Pruning
# ============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE="$(dirname "$SCRIPT_DIR")"
LOG_FILE="${WORKSPACE}/build/phase5_gate.log"
ARTIFACTS_DIR="/mnt/host/c/Users/mrasg/.gemini/antigravity-ide/brain/d0d1fb08-bc15-4e60-b76f-51e2393134ef"

mkdir -p "${WORKSPACE}/build"

echo "======================================================================"
echo "  Tinexus Phase 5 Gate: Multi-Select & Batch Operations Automated Gate"
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
chroot /mnt/rootfs /usr/bin/env PATH=/usr/bin:/bin ls /workspace/src/files/test_phase5_multiselect.cpp 2>&1 && echo "[+] test_phase5_multiselect.cpp visible in chroot"

# ── 3. Build test-phase5-multiselect target ─────────────────────────────────
echo ""
echo "[3/4] Building test-phase5-multiselect and tinexus-files..."
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
    rm -f /workspace/build/debug/bin/test-phase5-multiselect /workspace/build/debug/bin/tinexus-files
    cmake --build build/debug --target test-phase5-multiselect tinexus-files -j\$(nproc) 2>&1
    echo '[+] Build SUCCESS'
    ls -lh /workspace/build/debug/bin/test-phase5-multiselect /workspace/build/debug/bin/tinexus-files
" 2>&1 | tee "${LOG_FILE}"

# ── 4. Run Phase 5 Verification Suite ───────────────────────────────────────
echo ""
echo "[4/4] Executing Phase 5 automated test suite in chroot..."
chroot /mnt/rootfs /usr/bin/env PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin /usr/bin/bash -c "
    set -euo pipefail
    export QT_QPA_PLATFORM=offscreen
    export XDG_DATA_HOME=/root/.local/share
    export HOME=/root
    /workspace/build/debug/bin/test-phase5-multiselect
" 2>&1 | tee -a "${LOG_FILE}"

echo ""
echo "======================================================================"
echo "  [ALL PHASE 5 CHECKS PASSED] Log written to: ${LOG_FILE}"
echo "======================================================================"

if [ -d "${ARTIFACTS_DIR}" ]; then
    cp -f "${LOG_FILE}" "${ARTIFACTS_DIR}/phase5_gate.log" 2>/dev/null || true
    echo "[+] Log copied to artifacts directory: phase5_gate.log"
fi
