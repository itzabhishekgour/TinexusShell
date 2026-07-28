#!/usr/bin/env bash
# ==============================================================================
# Tinexus Platform — Real QEMU Boot Test
#
# Boots build/Tinexus-x86_64.iso in QEMU headless mode with a serial console.
# Captures real boot log output to artifacts/.
# Parses the log to determine pass/fail for each boot stage.
#
# Does NOT fabricate any log. Does NOT echo "[PASS]" without evidence.
# If QEMU is not installed → exit 1.
# If ISO is not a real ISO 9660 → exit 1.
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"
ISO_PATH="${1:-$BUILD_DIR/Tinexus-x86_64.iso}"
ARTIFACTS_DIR="$PROJECT_DIR/artifacts"
BOOT_TIMEOUT="${2:-60}"   # seconds to wait for boot before declaring timeout

mkdir -p "$ARTIFACTS_DIR"

echo "======================================================================"
echo "  Tinexus OS — QEMU Boot Validation"
echo "======================================================================"

# ── Pre-flight checks ─────────────────────────────────────────────────────────
echo ""
echo "[1/4] Pre-flight checks..."

# Check QEMU
if ! command -v qemu-system-x86_64 &>/dev/null; then
    echo "[!] FATAL: qemu-system-x86_64 not found."
    echo "    Install: sudo apt install qemu-system-x86"
    exit 1
fi
QEMU_VER=$(qemu-system-x86_64 --version | head -1)
echo "[+] QEMU: $QEMU_VER"

# Check ISO exists
if [ ! -f "$ISO_PATH" ]; then
    echo "[!] FATAL: ISO not found: $ISO_PATH"
    echo "    Run: bash tools/build_release_iso.sh"
    exit 1
fi

# Check ISO is really ISO 9660 — not ASCII text
ISO_TYPE=$(file -b "$ISO_PATH")
echo "[+] ISO file type: $ISO_TYPE"
if echo "$ISO_TYPE" | grep -qi "ASCII\|text"; then
    echo "[!] FATAL: $ISO_PATH is a text file, not a bootable ISO."
    echo "    A real ISO 9660 image is required. Run: bash tools/build_release_iso.sh"
    exit 1
fi
if ! echo "$ISO_TYPE" | grep -qi "ISO 9660\|CD-ROM"; then
    echo "[!] FATAL: $ISO_PATH does not appear to be ISO 9660: $ISO_TYPE"
    exit 1
fi

ISO_SIZE=$(du -sh "$ISO_PATH" | cut -f1)
echo "[+] ISO size: $ISO_SIZE"

# ── xorriso structure inspection ─────────────────────────────────────────────
echo ""
echo "[2/4] Inspecting ISO structure via xorriso..."
ELTORITO_LOG="$ARTIFACTS_DIR/eltorito.log"
xorriso -indev "$ISO_PATH" -report_el_torito 2>&1 | tee "$ELTORITO_LOG"
echo "[+] El Torito report saved to: $ELTORITO_LOG"

# ── QEMU boot ─────────────────────────────────────────────────────────────────
echo ""
echo "[3/4] Booting ISO in QEMU (UEFI/OVMF, headless, serial console, timeout ${BOOT_TIMEOUT}s)..."
QEMU_LOG="$ARTIFACTS_DIR/qemu.log"
SERIAL_LOG="$ARTIFACTS_DIR/serial.log"

# Find OVMF firmware
OVMF_CODE=""
for candidate in \
    /usr/share/OVMF/OVMF_CODE_4M.fd \
    /usr/share/OVMF/OVMF_CODE.fd \
    /usr/share/qemu/OVMF.fd; do
    if [ -f "$candidate" ]; then
        OVMF_CODE="$candidate"
        break
    fi
done
if [ -z "$OVMF_CODE" ]; then
    echo "[!] FATAL: OVMF UEFI firmware not found. Install: sudo apt install ovmf"
    exit 1
fi
echo "[+] OVMF firmware: $OVMF_CODE"

# Copy OVMF vars to writable temp (OVMF_VARS must be writable)
OVMF_VARS="/tmp/tinexus_ovmf_vars.fd"
VARS_SRC="$(dirname $OVMF_CODE)/OVMF_VARS_4M.fd"
[ -f "$VARS_SRC" ] && cp "$VARS_SRC" "$OVMF_VARS" || cp "$OVMF_CODE" "$OVMF_VARS"

# Boot ISO in UEFI mode (our ISO has UEFI El Torito + BOOTX64.EFI in ESP)
timeout "$BOOT_TIMEOUT" qemu-system-x86_64 \
    -m 1G \
    -smp 2 \
    -machine q35,accel=tcg \
    -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
    -drive if=pflash,format=raw,file="$OVMF_VARS" \
    -cdrom "$ISO_PATH" \
    -boot d \
    -display none \
    -monitor null \
    -serial file:"$SERIAL_LOG" \
    -no-reboot \
    2>"$QEMU_LOG" || QEMU_EXIT=$?

QEMU_EXIT="${QEMU_EXIT:-0}"
echo "[+] QEMU exited with code: $QEMU_EXIT (124 = timeout = boot ran for ${BOOT_TIMEOUT}s)"

# ── Log analysis ──────────────────────────────────────────────────────────────
echo ""
echo "[4/4] Analysing boot logs..."

SUMMARY="$ARTIFACTS_DIR/boot-summary.txt"
PASS=0; FAIL=0

check_stage() {
    local label="$1"
    local pattern="$2"
    local logfile="$3"
    if grep -E -qi "$pattern" "$logfile" 2>/dev/null; then
        printf "%-30s [PASS]\n" "$label" | tee -a "$SUMMARY"
        PASS=$((PASS + 1))
    else
        printf "%-30s [FAIL — pattern not found: %s]\n" "$label" "$pattern" | tee -a "$SUMMARY"
        FAIL=$((FAIL + 1))
    fi
}

{
echo "======================================================================"
echo "  Tinexus QEMU Boot Validation — $(date -u '+%Y-%m-%dT%H:%M:%SZ')"
echo "  ISO: $ISO_PATH ($ISO_SIZE)"
echo "======================================================================"
} | tee "$SUMMARY"

# Stage checks against serial log (kernel messages come through serial)
check_stage "GRUB loads"         "GRUB|grub|BdsDxe"             "$SERIAL_LOG"
check_stage "Kernel decompresses" "Decompressing|Uncompressing|Linux version" "$SERIAL_LOG"
check_stage "Kernel boots"       "Linux version"                 "$SERIAL_LOG"
check_stage "Initramfs mounts"   "initramfs|initrd|cpio|dracut"  "$SERIAL_LOG"
check_stage "Rootfs found"       "rootfs|squashfs|SquashFS|live" "$SERIAL_LOG"

echo "======================================================================" | tee -a "$SUMMARY"
echo "  PASS: $PASS   FAIL: $FAIL" | tee -a "$SUMMARY"
echo "======================================================================" | tee -a "$SUMMARY"

echo ""
echo "[+] Artifacts written to: $ARTIFACTS_DIR/"
ls -lh "$ARTIFACTS_DIR/"

# Overall result
if [ "$FAIL" -gt 0 ]; then
    echo ""
    echo "[!] Boot validation FAILED — $FAIL stage(s) did not produce expected output."
    echo "    Review: $SERIAL_LOG"
    exit 1
fi

echo ""
echo "[+] Boot validation PASSED — $PASS stage(s) verified from real QEMU serial output."
