#!/usr/bin/env bash
# ============================================================================
# test_phase1_gate.sh — Phase 1 Sort+Tag Gate: Build + Run in rootfs chroot
#
# Pattern: identical to compile_phase3_dbus.sh / test_native_app.sh
# No fabricated output. If binary crashes or assert fires → non-zero exit.
# ============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE="$(dirname "$SCRIPT_DIR")"
LOG_FILE="${WORKSPACE}/build/phase1_gate.log"
ARTIFACTS_DIR="/mnt/host/c/Users/mrasg/.gemini/antigravity-ide/brain/70aab369-09dc-4076-bf07-d86519e79211"

mkdir -p "${WORKSPACE}/build"

echo "======================================================================"
echo "  Tinexus Phase 1 Gate: Sort-by-column + Tag apply/persist"
echo "  $(date -u '+%Y-%m-%dT%H:%M:%SZ')"
echo "======================================================================"

# ── 1. Ensure mounts ────────────────────────────────────────────────────────
echo ""
echo "[1/5] Ensuring rootfs mounts..."
bash "${WORKSPACE}/tools/ensure_mounts.sh"
echo "[+] Mounts OK"

# ── 2. Sync latest source into /workspace inside rootfs ─────────────────────
# /workspace is bind-mounted to /workspace on the host by ensure_mounts.sh
# Source is already at /workspace (= ${WORKSPACE} on host)
echo ""
echo "[2/5] Verifying source sync..."
chroot /mnt/rootfs /usr/bin/env PATH=/usr/bin:/bin ls /workspace/src/files/test_phase1_sort_tag.cpp 2>&1 && echo "[+] test_phase1_sort_tag.cpp visible in chroot"

# ── 3. Build test-phase1-sort-tag target ────────────────────────────────────
echo ""
echo "[3/5] Building test-phase1-sort-tag (offscreen, no Wayland needed)..."
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
    cmake --build build/debug --target test-phase1-sort-tag -j\$(nproc) 2>&1
    echo '[+] Build SUCCESS'
    ls -lh /workspace/build/debug/bin/test-phase1-sort-tag
" 2>&1 | tee "${LOG_FILE}"

# ── 4. Run the in-process test suite ─────────────────────────────────────────
echo ""
echo "[4/5] Running Phase 1 gate test in chroot (offscreen platform)..."
chroot /mnt/rootfs /usr/bin/env PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin /usr/bin/bash -c "
    set -euo pipefail
    export QT_QPA_PLATFORM=offscreen
    export HOME=/root
    mkdir -p /root/.config/tinexus
    /workspace/build/debug/bin/test-phase1-sort-tag 2>&1
" 2>&1 | tee -a "${LOG_FILE}"

# ── 4b. True Cross-Process Cold-Start Persistence Verification ──────────────
echo ""
echo "[4b/5] True Cross-Process Cold-Start Persistence Check (separate process lifecycles)..."
chroot /mnt/rootfs /usr/bin/env PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin /usr/bin/bash -c "
    set -euo pipefail
    export QT_QPA_PLATFORM=offscreen
    export HOME=/root

    echo '  [PROC-1] Launching Process 1 to write tag \"Orange\" to /etc/hosts...'
    /workspace/build/debug/bin/test-phase1-sort-tag --tag-write /etc/hosts Orange
    echo '  [PROC-1] Process 1 exited cleanly. Checking tags.json on disk directly:'
    cat /root/.config/tinexus/tags.json

    echo '  [PROC-2] Launching Process 2 (completely fresh memory space)...'
    /workspace/build/debug/bin/test-phase1-sort-tag --tag-read /etc/hosts Orange
    echo '  [PROC-2] Process 2 verified tag directly from cold disk load!'

    echo '  [PROC-3] Launching Process 3 to clear tag...'
    /workspace/build/debug/bin/test-phase1-sort-tag --tag-clear /etc/hosts

    echo '  [PROC-4] Launching Process 4 to confirm cleared state...'
    /workspace/build/debug/bin/test-phase1-sort-tag --tag-read /etc/hosts ''
    echo '  [SUCCESS] Multi-process cold-start disk persistence verified.'
" 2>&1 | tee -a "${LOG_FILE}"

TEST_EXIT=${PIPESTATUS[0]}

# ── 5. Report ────────────────────────────────────────────────────────────────
echo ""
echo "[5/5] Results..."
echo ""
echo "--- tags.json on disk (inside rootfs) ---"
chroot /mnt/rootfs /usr/bin/env PATH=/usr/bin:/bin cat /root/.config/tinexus/tags.json 2>/dev/null || echo "(not found — cleared by test cleanup)"

# Copy log to artifacts dir so it's accessible from Windows side
mkdir -p "${ARTIFACTS_DIR}"
cp -f "${LOG_FILE}" "${ARTIFACTS_DIR}/phase1_gate.log" 2>/dev/null || true

echo ""
echo "======================================================================"
if [ "${TEST_EXIT}" -eq 0 ]; then
    echo "  PHASE 1 GATE: ALL CHECKS PASSED"
else
    echo "  PHASE 1 GATE: FAILED (exit code ${TEST_EXIT})"
    echo "  Full log: ${LOG_FILE}"
    exit 1
fi
echo "======================================================================"
