#!/usr/bin/env python3
"""
Tinexus Platform — Safe USB Flash Tool (Windows)
Flashes build/Tinexus-x86_64.iso directly to the target USB flash drive.
Includes strict safety checks to NEVER touch internal disks.
"""

import sys
import os
import time
import ctypes
from ctypes import wintypes
import subprocess
import json
from pathlib import Path

PROJECT_DIR = Path(__file__).resolve().parent.parent
ISO_PATH = PROJECT_DIR / "build" / "Tinexus-x86_64.iso"
TARGET_DISK_NUMBER = 1
MIN_USB_SIZE = 4 * 1024 * 1024 * 1024       # 4 GB
MAX_USB_SIZE = 64 * 1024 * 1024 * 1024      # 64 GB

def is_admin():
    try:
        return ctypes.windll.shell32.IsUserAnAdmin() != 0
    except:
        return False

def get_disk_info(disk_num):
    cmd = f'powershell -NoProfile -Command "Get-Disk -Number {disk_num} | Select Number, FriendlyName, BusType, Size, IsSystem, IsBoot | ConvertTo-Json"'
    out = subprocess.check_output(cmd, shell=True).decode("utf-8")
    return json.loads(out)

def dismount_disk_volumes(disk_num):
    print(f"[*] Dismounting existing volumes on Disk {disk_num}...")
    ps_cmd = f'''
    $parts = Get-Partition -DiskNumber {disk_num} -ErrorAction SilentlyContinue
    foreach ($p in $parts) {{
        if ($p.DriveLetter) {{
            Write-Host "Dismounting drive $($p.DriveLetter):"
            $vol = "\\\\.\\$($p.DriveLetter):"
            $h = [System.IO.File]::Open($vol, [System.IO.FileMode]::Open, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::ReadWrite)
            $h.Close()
        }}
    }}
    Clear-Disk -Number {disk_num} -RemoveData -Confirm:$false -ErrorAction SilentlyContinue
    '''
    subprocess.run(["powershell", "-NoProfile", "-Command", ps_cmd], capture_output=True)

def flash_iso():
    if not ISO_PATH.exists():
        print(f"[!] FATAL: ISO file not found at {ISO_PATH}")
        input("Press Enter to exit...")
        sys.exit(1)

    iso_size = ISO_PATH.stat().st_size
    print("=" * 65)
    print("           TINEXUS PLATFORM — SAFE USB FLASHER")
    print("=" * 65)
    print(f"[*] ISO Image : {ISO_PATH.name} ({iso_size / (1024*1024):.1f} MB)")

    # 1. Safety verification of target disk
    print(f"[*] Querying Disk {TARGET_DISK_NUMBER} hardware properties...")
    disk_info = get_disk_info(TARGET_DISK_NUMBER)
    print(f"    - FriendlyName : {disk_info.get('FriendlyName')}")
    print(f"    - BusType      : {disk_info.get('BusType')}")
    print(f"    - Size         : {disk_info.get('Size', 0) / (1024*1024*1024):.2f} GB")
    print(f"    - IsSystem     : {disk_info.get('IsSystem')}")
    print(f"    - IsBoot       : {disk_info.get('IsBoot')}")

    # Safety checks
    if disk_info.get("IsSystem") is True or disk_info.get("IsBoot") is True:
        print("[!] FATAL SAFETY ABORT: Target disk is marked as System or Boot disk!")
        input("Press Enter to exit...")
        sys.exit(1)

    bus_type = str(disk_info.get("BusType", "")).upper()
    if bus_type != "USB" and bus_type != "7":
        print(f"[!] FATAL SAFETY ABORT: Disk {TARGET_DISK_NUMBER} is not a USB drive (BusType: {bus_type})!")
        input("Press Enter to exit...")
        sys.exit(1)

    disk_size = disk_info.get("Size", 0)
    if disk_size < MIN_USB_SIZE or disk_size > MAX_USB_SIZE:
        print(f"[!] FATAL SAFETY ABORT: Disk size ({disk_size} bytes) outside expected USB range!")
        input("Press Enter to exit...")
        sys.exit(1)

    if disk_size < iso_size:
        print(f"[!] FATAL: USB drive is smaller than ISO image!")
        input("Press Enter to exit...")
        sys.exit(1)

    print("\n[+] Safety checks PASSED: Target is confirmed removable USB drive.")
    
    # 2. Dismount volumes
    dismount_disk_volumes(TARGET_DISK_NUMBER)
    time.sleep(1)

    # 3. Open raw disk for writing
    disk_path = rf"\\.\PhysicalDrive{TARGET_DISK_NUMBER}"
    print(f"[*] Opening raw device handle to {disk_path}...")
    
    GENERIC_READ = 0x80000000
    GENERIC_WRITE = 0x40000000
    FILE_SHARE_READ = 0x00000001
    FILE_SHARE_WRITE = 0x00000002
    OPEN_EXISTING = 3
    
    handle = ctypes.windll.kernel32.CreateFileW(
        disk_path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        None,
        OPEN_EXISTING,
        0,
        None
    )
    
    if handle in (-1, 0):
        err = ctypes.windll.kernel32.GetLastError()
        print(f"[!] Failed to open raw disk. Windows error: {err}")
        input("Press Enter to exit...")
        sys.exit(1)

    # 4. Stream write ISO to disk
    chunk_size = 4 * 1024 * 1024 # 4 MB
    bytes_written_total = 0
    start_time = time.time()
    
    print(f"[*] Flashing {iso_size / (1024*1024):.1f} MB to USB drive...")
    try:
        with open(ISO_PATH, "rb") as f_iso:
            buf = ctypes.create_string_buffer(chunk_size)
            written = wintypes.DWORD(0)
            
            while True:
                data = f_iso.read(chunk_size)
                if not data:
                    break
                
                # Write to disk handle
                ctypes.memmove(buf, data, len(data))
                success = ctypes.windll.kernel32.WriteFile(
                    handle,
                    buf,
                    len(data),
                    ctypes.byref(written),
                    None
                )
                
                if not success:
                    err = ctypes.windll.kernel32.GetLastError()
                    print(f"\n[!] WriteFile failed with Windows error: {err}")
                    break
                
                bytes_written_total += written.value
                pct = (bytes_written_total / iso_size) * 100.0
                elapsed = time.time() - start_time
                rate = (bytes_written_total / (1024 * 1024)) / (elapsed if elapsed > 0 else 1)
                sys.stdout.write(f"\r    [Progress] {pct:5.1f}% | {bytes_written_total/(1024*1024):.1f} / {iso_size/(1024*1024):.1f} MB | {rate:.1f} MB/s")
                sys.stdout.flush()

        print("\n[*] Flushing hardware write buffers...")
        ctypes.windll.kernel32.FlushFileBuffers(handle)
    finally:
        ctypes.windll.kernel32.CloseHandle(handle)

    total_time = time.time() - start_time
    print(f"\n[+] Flashing COMPLETE in {total_time:.1f}s ({bytes_written_total / (1024*1024):.1f} MB written).")
    print("[+] Rescanning disk partitions...")
    subprocess.run(["powershell", "-NoProfile", "-Command", f"Update-HostStorageCache; Get-Disk -Number {TARGET_DISK_NUMBER} | Format-Table"], capture_output=False)
    
    print("\n" + "=" * 65)
    print("SUCCESS: USB drive is ready for bare-metal UEFI boot testing!")
    print("=" * 65)
    time.sleep(2)

def main():
    if not is_admin():
        print("[*] Requesting Administrator privileges to access raw USB disk...")
        script_path = str(Path(__file__).resolve())
        # Re-launch with elevation
        ret = ctypes.windll.shell32.ShellExecuteW(
            None,
            "runas",
            sys.executable,
            f'"{script_path}"',
            None,
            1 # SW_SHOWNORMAL
        )
        if ret <= 32:
            print(f"[!] UAC prompt was cancelled or elevation failed (code {ret}).")
        else:
            print("[+] Elevated flashing process launched.")
        return

    flash_iso()

if __name__ == "__main__":
    main()
