#!/usr/bin/env bash
# ============================================================================
# test_phase2b_gate.sh — Phase 2B Gate: Interactive Collision Resolution
#
# Builds test-phase2b-conflict in rootfs chroot and executes all 7 verification checks:
#   CONF-1: Skip resolution leaves target untouched
#   CONF-2: Replace resolution overwrites destination with SHA-256 match
#   CONF-3: Keep Both resolution preserves original and creates suffixed file
#   CONF-4: Apply to All batch resolution with strictly 1 dialog prompt
#   CONF-5: Cancel while waiting on condition variable unblocks without deadlock
#   CONF-6: Directory-over-directory merges transparently
#   CONF-7: Type mismatch (file vs directory) populates warning & item count
# ============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE="$(dirname "$SCRIPT_DIR")"
LOG_FILE="${WORKSPACE}/build/phase2b_gate.log"
ARTIFACTS_DIR="/mnt/host/c/Users/mrasg/.gemini/antigravity-ide/brain/70aab369-09dc-4076-bf07-d86519e79211"

mkdir -p "${WORKSPACE}/build"

echo "======================================================================"
echo "  Tinexus Phase 2B Gate: Interactive Collision Resolution Test"
echo "  $(date -u '+%Y-%m-%dT%H:%M:%SZ')"
echo "======================================================================"

# ── 1. Ensure mounts ────────────────────────────────────────────────────────
echo ""
echo "[1/4] Ensuring rootfs mounts..."
bash "${WORKSPACE}/tools/ensure_mounts.sh"
echo "[+] Mounts OK"

# ── 2. Sync / Verify source ──────────────────────────────────────────────────
echo ""
echo "[2/4] Verifying source files in chroot..."
chroot /mnt/rootfs /usr/bin/env PATH=/usr/bin:/bin ls /workspace/src/files/test_phase2b_conflict.cpp 2>&1 && echo "[+] test_phase2b_conflict.cpp visible in chroot"

# ── 3. Build test-phase2b-conflict target ────────────────────────────────────
echo ""
echo "[3/4] Building test-phase2b-conflict and tinexus-files..."
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
    cmake --build build/debug --target test-phase2b-conflict tinexus-files -j\$(nproc) 2>&1
    echo '[+] Build SUCCESS'
    ls -lh /workspace/build/debug/bin/test-phase2b-conflict /workspace/build/debug/bin/tinexus-files
" 2>&1 | tee "${LOG_FILE}"

# ── 4. Run Phase 2B Verification Suite ───────────────────────────────────────
echo ""
echo "[4/4] Executing Phase 2B automated test suite in chroot..."
chroot /mnt/rootfs /usr/bin/env PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin /usr/bin/bash -c "
    set -euo pipefail
    export QT_QPA_PLATFORM=offscreen
    export HOME=/root
    /workspace/build/debug/bin/test-phase2b-conflict 2>&1
" 2>&1 | tee -a "${LOG_FILE}"

TEST_EXIT=${PIPESTATUS[0]}

# Copy log to artifacts dir so it's accessible from Windows side
mkdir -p "${ARTIFACTS_DIR}"
cp -f "${LOG_FILE}" "${ARTIFACTS_DIR}/phase2b_gate.log" 2>/dev/null || true

echo ""
echo "======================================================================"
if [ "${TEST_EXIT}" -eq 0 ]; then
    echo "  PHASE 2B GATE: ALL VERIFICATIONS PASSED"
else
    echo "  PHASE 2B GATE: FAILED (exit code ${TEST_EXIT})"
    echo "  Full log: ${LOG_FILE}"
    exit 1
fi
echo "======================================================================"
