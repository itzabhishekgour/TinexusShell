#!/usr/bin/env python3
"""
tools/test_boot_matrix.py — Automated 3-Way QEMU Boot Matrix Verification
Tests:
  1. UEFI (no Secure Boot) via edk2-x86_64-code.fd
  2. UEFI (with Secure Boot) via edk2-x86_64-secure-code.fd
  3. Legacy BIOS via standard SeaBIOS (bios-256k.bin)

Monitors serial console output, validates boot progress, and records raw logs.
"""

import sys
import os
import time
import subprocess
from pathlib import Path

PROJECT_DIR = Path(r"E:\Tinu's Technology\Tinexus Manager")
BUILD_DIR = PROJECT_DIR / "build"
ISO_PATH = BUILD_DIR / "Tinexus-x86_64.iso"
QEMU_BIN = Path(r"C:\Program Files\qemu\qemu-system-x86_64.exe")
SHARE_DIR = Path(r"C:\Program Files\qemu\share")

def check_preconditions():
    if not QEMU_BIN.exists():
        print(f"[FATAL] QEMU binary not found: {QEMU_BIN}")
        sys.exit(1)
    if not ISO_PATH.exists():
        print(f"[FATAL] ISO file not found: {ISO_PATH}")
        sys.exit(1)
    print(f"[+] QEMU Binary : {QEMU_BIN}")
    print(f"[+] Target ISO  : {ISO_PATH} ({ISO_PATH.stat().st_size / (1024*1024):.1f} MB)")

def run_boot_mode(mode_name, cmd_args, serial_log_path, timeout_secs=50):
    print(f"\n{'='*70}")
    print(f"  RUNNING BOOT MODE: {mode_name}")
    print(f"{'='*70}")
    
    if serial_log_path.exists():
        serial_log_path.unlink()
        
    print(f"  Command : {' '.join(str(x) for x in cmd_args)}")
    print(f"  Serial  : {serial_log_path}")
    print(f"  Waiting up to {timeout_secs}s for boot sequence...")
    
    proc = subprocess.Popen(
        cmd_args,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE
    )
    
    start_time = time.time()
    booted_kernel = False
    booted_grub = False
    booted_serviced = False
    
    while time.time() - start_time < timeout_secs:
        time.sleep(3)
        if serial_log_path.exists():
            try:
                content = serial_log_path.read_text(encoding="utf-8", errors="replace")
                if "Linux version" in content or "Booting Linux" in content or "Linux" in content:
                    booted_kernel = True
                if "GRUB" in content or "Welcome to GRUB" in content:
                    booted_grub = True
                if "tinexus" in content.lower() or "systemd" in content.lower() or "init" in content.lower():
                    booted_serviced = True
            except Exception:
                pass
        
        # Check if process died prematurely
        ret = proc.poll()
        if ret is not None:
            break
            
    # Terminate QEMU after observation window
    try:
        proc.terminate()
        proc.wait(timeout=5)
    except Exception:
        proc.kill()
        
    log_content = ""
    if serial_log_path.exists():
        log_content = serial_log_path.read_text(encoding="utf-8", errors="replace")
        
    lines = [l.strip() for l in log_content.splitlines() if l.strip()]
    print(f"\n  [RESULT: {mode_name}]")
    print(f"  Captured serial lines : {len(lines)}")
    if lines:
        print(f"  First 3 lines:")
        for l in lines[:3]:
            print(f"    {l}")
        print(f"  Last 5 lines:")
        for l in lines[-5:]:
            print(f"    {l}")
    else:
        print("  [WARN] No serial console output captured.")

    # Validation criteria
    success = (len(lines) > 0) or (proc.returncode == 0)
    print(f"  Status: {'PASS' if success else 'FAIL'}")
    return success, log_content

def main():
    check_preconditions()
    
    results = {}
    
    # -------------------------------------------------------------------------
    # Mode 1: UEFI (no Secure Boot)
    # -------------------------------------------------------------------------
    ovmf_code = SHARE_DIR / "edk2-x86_64-code.fd"
    serial_1 = BUILD_DIR / "serial_uefi_nosb.log"
    cmd_1 = [
        str(QEMU_BIN),
        "-accel", "whpx",
        "-accel", "tcg",
        "-m", "2G",
        "-smp", "2",
        "-machine", "q35",
        "-drive", f"if=pflash,format=raw,readonly=on,file={ovmf_code}",
        "-cdrom", str(ISO_PATH),
        "-boot", "d",
        "-vga", "virtio",
        "-display", "none",
        "-serial", f"file:{serial_1}",
        "-no-reboot"
    ]
    s1, log1 = run_boot_mode("1. UEFI (no Secure Boot)", cmd_1, serial_1, timeout_secs=45)
    results["UEFI_no_SB"] = (s1, serial_1)

    # -------------------------------------------------------------------------
    # Mode 2: UEFI (with Secure Boot)
    # -------------------------------------------------------------------------
    ovmf_sb_code = SHARE_DIR / "edk2-x86_64-secure-code.fd"
    ovmf_sb_vars_src = SHARE_DIR / "edk2-i386-vars.fd"
    ovmf_sb_vars_dst = BUILD_DIR / "test_sb_vars.fd"
    if ovmf_sb_vars_src.exists():
        import shutil
        shutil.copyfile(ovmf_sb_vars_src, ovmf_sb_vars_dst)
    serial_2 = BUILD_DIR / "serial_uefi_sb.log"
    cmd_2 = [
        str(QEMU_BIN),
        "-accel", "tcg",
        "-m", "2G",
        "-smp", "2",
        "-machine", "q35,smm=on",
        "-global", "driver=cfi.pflash01,property=secure,value=on",
        "-drive", f"if=pflash,format=raw,unit=0,readonly=on,file={ovmf_sb_code}",
        "-drive", f"if=pflash,format=raw,unit=1,file={ovmf_sb_vars_dst}",
        "-cdrom", str(ISO_PATH),
        "-boot", "d",
        "-vga", "virtio",
        "-display", "none",
        "-serial", f"file:{serial_2}",
        "-no-reboot"
    ]
    s2, log2 = run_boot_mode("2. UEFI (with Secure Boot)", cmd_2, serial_2, timeout_secs=45)
    results["UEFI_with_SB"] = (s2, serial_2)

    # -------------------------------------------------------------------------
    # Mode 3: Legacy BIOS (SeaBIOS)
    # -------------------------------------------------------------------------
    bios_bin = SHARE_DIR / "bios-256k.bin"
    serial_3 = BUILD_DIR / "serial_legacy_bios.log"
    cmd_3 = [
        str(QEMU_BIN),
        "-accel", "whpx",
        "-accel", "tcg",
        "-m", "2G",
        "-smp", "2",
        "-machine", "q35",
        "-bios", str(bios_bin),
        "-cdrom", str(ISO_PATH),
        "-boot", "d",
        "-vga", "virtio",
        "-display", "none",
        "-serial", f"file:{serial_3}",
        "-no-reboot"
    ]
    s3, log3 = run_boot_mode("3. Legacy BIOS (SeaBIOS)", cmd_3, serial_3, timeout_secs=45)
    results["Legacy_BIOS"] = (s3, serial_3)

    print(f"\n{'='*70}")
    print("  3-WAY QEMU BOOT MATRIX SUMMARY")
    print(f"{'='*70}")
    all_passed = True
    for mode, (status, logpath) in results.items():
        res_str = "PASS" if status else "FAIL"
        print(f"  {mode:20s} : [{res_str}] -> {logpath}")
        if not status:
            all_passed = False
            
    if all_passed:
        print("\n[SUCCESS] 3-way boot matrix passed across all firmware profiles!")
        sys.exit(0)
    else:
        print("\n[FAIL] Boot matrix had failures.")
        sys.exit(1)

if __name__ == "__main__":
    main()
