#!/usr/bin/env bash
# ============================================================================
# test_phase3_gate.sh — Phase 3 Gate: Search & Address Bar Verification
#
# Builds test-phase3-search in rootfs chroot and executes all 5 verification checks:
#   SRCH-1: Exact & Substring Search Match
#   SRCH-2: Recursive Deep-Tree Search & Relative Path Mapping
#   SRCH-3: Rapid Query Debounce & Cooperative Cancellation
#   SRCH-4: Address Bar Tilde (~), Relative, and Error Path Navigation
#   SRCH-5: View Transparency & Seamless Model Swapping
# ============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE="$(dirname "$SCRIPT_DIR")"
LOG_FILE="${WORKSPACE}/build/phase3_gate.log"
ARTIFACTS_DIR="/mnt/host/c/Users/mrasg/.gemini/antigravity-ide/brain/70aab369-09dc-4076-bf07-d86519e79211"

mkdir -p "${WORKSPACE}/build"

echo "======================================================================"
echo "  Tinexus Phase 3 Gate: Search & Address Bar Automated Gate"
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
chroot /mnt/rootfs /usr/bin/env PATH=/usr/bin:/bin ls /workspace/src/files/test_phase3_search.cpp 2>&1 && echo "[+] test_phase3_search.cpp visible in chroot"

# ── 3. Build test-phase3-search target ──────────────────────────────────────
echo ""
echo "[3/4] Building test-phase3-search and tinexus-files..."
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
    cmake --build build/debug --target test-phase3-search tinexus-files -j\$(nproc) 2>&1
    echo '[+] Build SUCCESS'
    ls -lh /workspace/build/debug/bin/test-phase3-search /workspace/build/debug/bin/tinexus-files
" 2>&1 | tee "${LOG_FILE}"

# ── 4. Run Phase 3 Verification Suite ───────────────────────────────────────
echo ""
echo "[4/4] Executing Phase 3 automated test suite in chroot..."
chroot /mnt/rootfs /usr/bin/env PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin /usr/bin/bash -c "
    set -euo pipefail
    export QT_QPA_PLATFORM=offscreen
    export XDG_DATA_HOME=/root/.local/share
    export HOME=/root
    /workspace/build/debug/bin/test-phase3-search
" 2>&1 | tee -a "${LOG_FILE}"

echo ""
echo "======================================================================"
echo "  [ALL PHASE 3 CHECKS PASSED] Log written to: ${LOG_FILE}"
echo "======================================================================"

# Copy log to artifacts if accessible
if [ -d "${ARTIFACTS_DIR}" ]; then
    cp -f "${LOG_FILE}" "${ARTIFACTS_DIR}/phase3_gate.log" 2>/dev/null || true
    echo "[+] Log copied to artifacts directory: phase3_gate.log"
fi
