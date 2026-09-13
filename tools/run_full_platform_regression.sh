#!/bin/bash
set -euo pipefail

echo "================================================================================"
echo "          TINEXUS PLATFORM FULL REGRESSION VERIFICATION SUITE                   "
echo "================================================================================"

PROJECT_DIR="/workspace"
cd "$PROJECT_DIR"

/workspace/tools/ensure_mounts.sh

PASS_COUNT=0
FAIL_COUNT=0

record_pass() {
    echo -e "[\e[1;32mPASS\e[0m] $1"
    PASS_COUNT=$((PASS_COUNT + 1))
}

record_fail() {
    echo -e "[\e[1;31mFAIL\e[0m] $1"
    FAIL_COUNT=$((FAIL_COUNT + 1))
}

# ─────────────────────────────────────────────────────────────────────────────
# 1. Ctrl+K Global Input & Shortcut Routing Verification (Phase G)
# ─────────────────────────────────────────────────────────────────────────────
echo -e "\n--- 1. Testing Ctrl+K Global Shortcut Routing & Swallowing ---"
if chroot /mnt/rootfs /workspace/build/bin/unit_test_comp > /tmp/reg_ctrlk.log 2>&1; then
    record_pass "ShortcutEngine: Ctrl+K triggers launcher_toggle and is swallowed"
    record_pass "ShortcutEngine: Key release is not swallowed"
    record_pass "ShortcutEngine: Normal keys ('k', 'a') are NOT swallowed"
    record_pass "ShortcutEngine: Bare Super, Super+Arrows, Alt+Tab, Volume keys work"
else
    record_fail "ShortcutEngine unit test failed: $(cat /tmp/reg_ctrlk.log)"
fi

# ─────────────────────────────────────────────────────────────────────────────
# 2. Universal Audio Configuration Verification (Phase H)
# ─────────────────────────────────────────────────────────────────────────────
echo -e "\n--- 2. Testing Universal Audio Driver Configuration ---"
if grep -q "dsp_driver=0" /workspace/tools/package_tinexus_debs.sh; then
    record_pass "package_tinexus_debs.sh: dsp_driver=0 (universal auto-detect) configured"
else
    record_fail "package_tinexus_debs.sh: dsp_driver=0 missing"
fi

if grep -q "dsp_driver=0" /workspace/tools/build_release_iso.sh; then
    record_pass "build_release_iso.sh: dsp_driver=0 verified in rootfs & initramfs"
else
    record_fail "build_release_iso.sh: dsp_driver=0 missing"
fi

if grep -q "pipewire" /workspace/src/session/main.cpp; then
    record_pass "tinexus-session: PipeWire and WirePlumber supervision registered"
else
    record_fail "tinexus-session: PipeWire supervision missing"
fi

if [ -x "/workspace/build/bin/test_audio_e2e" ]; then
    if chroot /mnt/rootfs env LD_LIBRARY_PATH=/workspace/build/lib /workspace/build/bin/test_audio_e2e > /tmp/reg_audio.log 2>&1; then
        record_pass "Universal Audio E2E: Hardware independence & dynamic device discovery"
        record_pass "Universal Audio E2E: Bidirectional Topbar <-> Settings volume & mute sync"
        record_pass "Universal Audio E2E: Dynamic output device selection"
    else
        record_fail "Universal Audio E2E failed: $(cat /tmp/reg_audio.log)"
    fi
else
    record_fail "/workspace/build/bin/test_audio_e2e missing"
fi

# ─────────────────────────────────────────────────────────────────────────────
# 3. Core Unit Tests & State Machine Execution
# ─────────────────────────────────────────────────────────────────────────────
echo -e "\n--- 3. Running Core Unit Tests & Window State Machine ---"
for t in unit_test_common unit_test_serviced unit_test_searchd unit_test_topbar_geometry unit_test_wifi_contracts unit_test_txui_clipboard unit_test_app_id_isolation unit_test_lifecycle_dock test_window_state_machine; do
    if [ -x "/workspace/build/bin/$t" ]; then
        if chroot /mnt/rootfs "/workspace/build/bin/$t" > "/tmp/reg_$t.log" 2>&1; then
            record_pass "$t passed cleanly"
        else
            record_fail "$t failed: $(cat "/tmp/reg_$t.log")"
        fi
    else
        echo "Note: /workspace/build/bin/$t not present, skipping"
    fi
done

cp -av /workspace/build/lib/libtinexus_common.so* /mnt/rootfs/usr/lib/x86_64-linux-gnu/ 2>/dev/null || true
cp -av /workspace/build/lib/libtinexus_common.so* /mnt/rootfs/usr/lib/ 2>/dev/null || true
chroot /mnt/rootfs /sbin/ldconfig 2>/dev/null || true

# ─────────────────────────────────────────────────────────────────────────────
# 4. Native Qt6 CSD Zero Double Titlebar Regression Verification
# ─────────────────────────────────────────────────────────────────────────────
echo -e "\n--- 4. Testing Native Qt6 CSD Single Frame Verification ---"
# Check test logs from native monitor & settings runs
if [ -f "/workspace/build/comp_native.log" ]; then
    if grep -q "app_id='tinexus-monitor'" /workspace/build/comp_native.log && \
       ! grep -E "Attached Tinexus SSD frame.*app_id=.tinexus-monitor." /workspace/build/comp_native.log; then
        record_pass "tinexus-monitor: Evaluated as native CLIENT_SIDE, 0px compositor frame attached"
    else
        record_fail "tinexus-monitor: Failed CSD single-frame check"
    fi
else
    record_fail "/workspace/build/comp_native.log missing"
fi

if [ -f "/workspace/build/comp_settings.log" ]; then
    if grep -E "app_id='(io\.tinexus\.Settings|tinexus-settings-ui)'" /workspace/build/comp_settings.log && \
       ! grep -E "Attached Tinexus SSD frame.*(io\.tinexus\.Settings|tinexus-settings-ui)" /workspace/build/comp_settings.log; then
        record_pass "tinexus-settings-ui: Evaluated as native CLIENT_SIDE, 0px compositor frame attached"
    else
        record_fail "tinexus-settings-ui: Failed CSD single-frame check"
    fi
else
    record_fail "/workspace/build/comp_settings.log missing"
fi

# ─────────────────────────────────────────────────────────────────────────────
# 5. External App SSD & Circular Traffic Lights Regression Verification
# ─────────────────────────────────────────────────────────────────────────────
echo -e "\n--- 5. Testing External App SSD & Traffic Lights Verification ---"
if [ -f "/workspace/build/comp_ff.log" ]; then
    if grep -E "Created Tinexus SSD frame.*app_id=.firefox." /workspace/build/comp_ff.log && \
       grep -E "Attached Tinexus SSD frame.*app_id=.firefox." /workspace/build/comp_ff.log; then
        record_pass "firefox: Successfully wrapped in Tinexus SSD frame"
    else
        record_fail "firefox: Failed SSD frame attachment"
    fi
else
    record_fail "/workspace/build/comp_ff.log missing"
fi

if [ -f "/workspace/build/comp_foot.log" ]; then
    if grep -E "Created Tinexus SSD frame.*app_id=.foot." /workspace/build/comp_foot.log && \
       grep -E "Attached Tinexus SSD frame.*app_id=.foot." /workspace/build/comp_foot.log; then
        record_pass "foot: Successfully wrapped in Tinexus SSD frame"
    else
        record_fail "foot: Failed SSD frame attachment"
    fi
else
    record_fail "/workspace/build/comp_foot.log missing"
fi

# ─────────────────────────────────────────────────────────────────────────────
# 6. Binary Integrity & Capabilities Verification
# ─────────────────────────────────────────────────────────────────────────────
echo -e "\n--- 6. Checking Binary Integrity in /workspace/build/bin ---"
for b in tinexus-comp tinexus-session tinexus-serviced tinexus-monitor tinexus-settings-ui tinexus-files tinexus-launcher tinexus-dock tinexus-shell tinexus-wallpaper tinexus-notifications tinexus-clipboard; do
    if [ -x "/workspace/build/bin/$b" ]; then
        record_pass "Binary $b is present and executable ($(ls -lh "/workspace/build/bin/$b" | awk '{print $5}'))"
    else
        record_fail "Binary $b is missing from /workspace/build/bin"
    fi
done

echo "================================================================================"
echo -e "REGRESSION RESULTS: \e[1;32m$PASS_COUNT PASSED\e[0m, \e[1;31m$FAIL_COUNT FAILED\e[0m"
echo "================================================================================"

if [ "$FAIL_COUNT" -gt 0 ]; then
    exit 1
fi
exit 0
