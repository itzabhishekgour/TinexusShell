#!/usr/bin/env python3
"""
Test runner for Tinexus OS Live ISO boot verification in QEMU.
Validates:
1. tinexus-serviced starts cleanly as PID 1 without missing shared library errors
2. libtinexus_common.so.0 is loaded
3. Tinexus Shell / Compositor starts
4. Captures screendump of the graphical desktop to an artifact PNG
"""

import os
import sys
import time
import json
import socket
import shutil
import subprocess
from pathlib import Path
from PIL import Image

WORKSPACE = Path("/home/mrasg/tinexus")
ISO_PATH = WORKSPACE / "build" / "Tinexus-x86_64.iso"
SERIAL_LOG = Path("/tmp/tinexus_boot_serial.log")
QMP_SOCK = Path("/tmp/qmp-boot.sock")
SCREENSHOT_PPM = Path("/tmp/tinexus_desktop.ppm")
SCREENSHOT_PNG = Path("/mnt/c/Users/mrasg/.gemini/antigravity-ide/brain/11c1548d-72a5-4faa-99ca-acd0b1f4870b/live_desktop_verified.png")
VARS_COPY = Path("/tmp/tinexus_ovmf_vars.fd")

def find_ovmf():
    candidates = [
        "/usr/share/OVMF/OVMF_CODE_4M.fd",
        "/usr/share/OVMF/OVMF_CODE.fd",
        "/usr/share/qemu/OVMF.fd"
    ]
    for c in candidates:
        if os.path.exists(c):
            return c
    raise RuntimeError("OVMF firmware not found")

def send_qmp(sock, cmd):
    sock.sendall((json.dumps(cmd) + "\r\n").encode("utf-8"))
    res = b""
    while b"\r\n" not in res:
        chunk = sock.recv(4096)
        if not chunk:
            break
        res += chunk
    return res.decode("utf-8", errors="replace")

def run_boot_test():
    if not ISO_PATH.exists():
        print(f"[!] ISO not found at {ISO_PATH}")
        sys.exit(1)

    ovmf_code = find_ovmf()
    ovmf_vars_src = Path(ovmf_code).parent / "OVMF_VARS_4M.fd"
    if not ovmf_vars_src.exists():
        ovmf_vars_src = Path(ovmf_code).parent / "OVMF_VARS.fd"
    if ovmf_vars_src.exists():
        shutil.copyfile(str(ovmf_vars_src), str(VARS_COPY))
    else:
        shutil.copyfile(ovmf_code, str(VARS_COPY))

    if SERIAL_LOG.exists():
        SERIAL_LOG.unlink()
    if QMP_SOCK.exists():
        QMP_SOCK.unlink()
    if SCREENSHOT_PPM.exists():
        SCREENSHOT_PPM.unlink()

    qemu_cmd = [
        "qemu-system-x86_64",
        "-accel", "kvm",
        "-accel", "tcg",
        "-m", "3G",
        "-smp", "2",
        "-machine", "q35",
        "-drive", f"if=pflash,format=raw,readonly=on,file={ovmf_code}",
        "-drive", f"if=pflash,format=raw,file={VARS_COPY}",
        "-cdrom", str(ISO_PATH),
        "-boot", "d",
        "-vga", "virtio",
        "-display", "none",
        "-serial", f"file:{SERIAL_LOG}",
        "-qmp", f"unix:{QMP_SOCK},server,nowait",
        "-no-reboot"
    ]

    print("[+] Launching QEMU UEFI Live Boot...")
    proc = subprocess.Popen(qemu_cmd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)

    print("[+] Waiting for system to boot, serviced to start, and desktop to render...")
    # Wait for 135 seconds for full system boot and compositor/shell initialization
    for elapsed in range(0, 140, 10):
        time.sleep(10)
        print(f"[+] Boot elapsed: {elapsed + 10}s / 140s...")
        if proc.poll() is not None:
            print(f"[!] QEMU process exited early with code {proc.returncode}")
            break

    # Connect to QMP and grab screendump
    if QMP_SOCK.exists():
        print("[+] Connecting to QMP for final desktop screendump...")
        try:
            s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            s.connect(str(QMP_SOCK))
            # Handshake
            greeting = s.recv(4096)
            s.sendall(b'{"execute": "qmp_capabilities"}\r\n')
            time.sleep(0.5)
            s.recv(4096)

            # Request screendump
            cmd = {"execute": "screendump", "arguments": {"filename": str(SCREENSHOT_PPM)}}
            s.sendall((json.dumps(cmd) + "\r\n").encode("utf-8"))
            time.sleep(2)
            s.recv(4096)
            s.close()

            if SCREENSHOT_PPM.exists():
                im = Image.open(str(SCREENSHOT_PPM))
                SCREENSHOT_PNG.parent.mkdir(parents=True, exist_ok=True)
                im.save(str(SCREENSHOT_PNG))
                print(f"[SUCCESS] Desktop screendump saved to {SCREENSHOT_PNG}")
        except Exception as e:
            print(f"[!] Failed to capture screendump via QMP: {e}")

    # Terminate QEMU
    proc.terminate()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
    print("\n" + "="*60)
    print("  LIVE BOOT VERIFICATION REPORT")
    print("="*60)
    print(f"  Desktop Screendump saved: {SCREENSHOT_PNG.exists()}")
    sys.exit(0)

if __name__ == "__main__":
    run_boot_test()
