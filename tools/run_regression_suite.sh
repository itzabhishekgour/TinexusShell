#!/usr/bin/env bash
# ============================================================================
# run_regression_suite.sh — Full Regression Suite for Tinexus Files Phases 1-7
# Runs all 8 test gates inside rootfs chroot to guarantee zero regressions.
# ============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE="$(dirname "$SCRIPT_DIR")"
LOG_FILE="${WORKSPACE}/build/regression_suite.log"
ARTIFACTS_DIR="/mnt/host/c/Users/mrasg/.gemini/antigravity-ide/brain/d0d1fb08-bc15-4e60-b76f-51e2393134ef"

mkdir -p "${WORKSPACE}/build"

echo "======================================================================" | tee "${LOG_FILE}"
echo "  Tinexus Platform: Full End-to-End Regression Suite (Phases 1-7)"     | tee -a "${LOG_FILE}"
echo "  $(date -u '+%Y-%m-%dT%H:%M:%SZ')"                                    | tee -a "${LOG_FILE}"
echo "======================================================================" | tee -a "${LOG_FILE}"

# 1. Mounts
echo "" | tee -a "${LOG_FILE}"
echo "[1/3] Ensuring rootfs mounts..." | tee -a "${LOG_FILE}"
bash "${WORKSPACE}/tools/ensure_mounts.sh"
echo "[+] Mounts OK" | tee -a "${LOG_FILE}"

# 2. Build all test binaries
echo "" | tee -a "${LOG_FILE}"
echo "[2/3] Building all test gate binaries..." | tee -a "${LOG_FILE}"
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
    cmake --build build/debug --target \
        test-phase1-sort-tag \
        test-phase2-async \
        test-phase2b-conflict \
        test-phase3-search \
        test-phase4-tabs \
        test-phase5-multiselect \
        test-phase6-watcher \
        test-phase7-shortcuts \
        tinexus-files \
        -j\$(nproc) 2>&1
    echo '[+] All 8 test binaries compiled successfully'
" 2>&1 | tee -a "${LOG_FILE}"

# 3. Execute all 8 test gates in sequence
echo "" | tee -a "${LOG_FILE}"
echo "[3/3] Executing all test gates sequentially..." | tee -a "${LOG_FILE}"

GATES=(
    "Phase 1 (Sort & Tag):/workspace/build/debug/bin/test-phase1-sort-tag"
    "Phase 2A (Async Engine):/workspace/build/debug/bin/test-phase2-async"
    "Phase 2B (Conflict Resolution):/workspace/build/debug/bin/test-phase2b-conflict"
    "Phase 3 (Search & Address Bar):/workspace/build/debug/bin/test-phase3-search"
    "Phase 4 (Multi-Tab Model):/workspace/build/debug/bin/test-phase4-tabs"
    "Phase 5 (Multi-Select & Batch):/workspace/build/debug/bin/test-phase5-multiselect"
    "Phase 6 (inotify Live Watcher):/workspace/build/debug/bin/test-phase6-watcher"
    "Phase 7 (Keyboard Shortcuts):/workspace/build/debug/bin/test-phase7-shortcuts"
)

for entry in "${GATES[@]}"; do
    IFS=':' read -r NAME BIN <<< "$entry"
    echo "" | tee -a "${LOG_FILE}"
    echo "======================================================================" | tee -a "${LOG_FILE}"
    echo "  RUNNING GATE: ${NAME}" | tee -a "${LOG_FILE}"
    echo "======================================================================" | tee -a "${LOG_FILE}"
    chroot /mnt/rootfs /usr/bin/env PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin /usr/bin/bash -c "
        set -euo pipefail
        export QT_QPA_PLATFORM=offscreen
        export XDG_DATA_HOME=/root/.local/share
        export HOME=/root
        ${BIN}
    " 2>&1 | tee -a "${LOG_FILE}"
    echo "[PASS] ${NAME} passed completely!" | tee -a "${LOG_FILE}"
done

echo "" | tee -a "${LOG_FILE}"
echo "======================================================================" | tee -a "${LOG_FILE}"
echo "  [ALL REGRESSION GATES PASSED: 100% SUCCESS ACROSS ALL 8 TEST SUITES] " | tee -a "${LOG_FILE}"
echo "======================================================================" | tee -a "${LOG_FILE}"

if [ -d "${ARTIFACTS_DIR}" ]; then
    cp -f "${LOG_FILE}" "${ARTIFACTS_DIR}/regression_suite.log" 2>/dev/null || true
    echo "[+] Log copied to artifacts directory: regression_suite.log"
fi
