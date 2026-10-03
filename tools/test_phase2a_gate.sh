#!/usr/bin/env bash
# ============================================================================
# test_phase2a_gate.sh — Phase 2A Gate: Async File Operations & Telemetry
#
# Builds test-phase2-async in rootfs chroot and executes all 6 verification checks:
#   ASYNC-1: Main thread event loop latency (< 200ms) under 100MB copy
#   ASYNC-2: Monotonic progress & telemetry
#   ASYNC-3: SHA-256 bitwise file integrity
#   ASYNC-4: Cooperative cancellation & partial file removal
#   ASYNC-5: Safe collision auto-suffixing
#   ASYNC-6: Safe Trash integration
# ============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE="$(dirname "$SCRIPT_DIR")"
LOG_FILE="${WORKSPACE}/build/phase2a_gate.log"
ARTIFACTS_DIR="/mnt/host/c/Users/mrasg/.gemini/antigravity-ide/brain/70aab369-09dc-4076-bf07-d86519e79211"

mkdir -p "${WORKSPACE}/build"

echo "======================================================================"
echo "  Tinexus Phase 2A Gate: Async File Engine & GUI Non-Blocking Test"
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
chroot /mnt/rootfs /usr/bin/env PATH=/usr/bin:/bin ls /workspace/src/files/test_phase2_async.cpp 2>&1 && echo "[+] test_phase2_async.cpp visible in chroot"

# ── 3. Build test-phase2-async target ───────────────────────────────────────
echo ""
echo "[3/4] Building test-phase2-async and tinexus-files..."
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
    cmake --build build/debug --target test-phase2-async tinexus-files -j\$(nproc) 2>&1
    echo '[+] Build SUCCESS'
    ls -lh /workspace/build/debug/bin/test-phase2-async /workspace/build/debug/bin/tinexus-files
" 2>&1 | tee "${LOG_FILE}"

# ── 4. Run Phase 2A Verification Suite ───────────────────────────────────────
echo ""
echo "[4/4] Executing Phase 2A automated test suite in chroot..."
chroot /mnt/rootfs /usr/bin/env PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin /usr/bin/bash -c "
    set -euo pipefail
    export QT_QPA_PLATFORM=offscreen
    export HOME=/root
    mkdir -p /root/.config/tinexus /root/.local/share/Trash/files
    /workspace/build/debug/bin/test-phase2-async 2>&1
" 2>&1 | tee -a "${LOG_FILE}"

TEST_EXIT=${PIPESTATUS[0]}

# Copy log to artifacts dir so it's accessible from Windows side
mkdir -p "${ARTIFACTS_DIR}"
cp -f "${LOG_FILE}" "${ARTIFACTS_DIR}/phase2a_gate.log" 2>/dev/null || true

echo ""
echo "======================================================================"
if [ "${TEST_EXIT}" -eq 0 ]; then
    echo "  PHASE 2A GATE: ALL VERIFICATIONS PASSED"
else
    echo "  PHASE 2A GATE: FAILED (exit code ${TEST_EXIT})"
    echo "  Full log: ${LOG_FILE}"
    exit 1
fi
echo "======================================================================"
