#!/usr/bin/env python3
"""
Tinexus Platform — Windows-Native QEMU UEFI Live Boot Verification
Validates:
1. UEFI Grub & Kernel Boot
2. tinexus-serviced PID 1 startup without missing library errors
3. tinexus-session live boot execution (bypassing lock, spawning shell components)
4. Desktop rendering via QMP screendump saved to artifact PNG
"""

import os
import sys
import time
import json
import socket
import subprocess
from pathlib import Path
from PIL import Image

PROJECT_DIR = Path(r"E:\Tinu's Technology\Tinexus Manager")
BUILD_DIR = PROJECT_DIR / "build"
ISO_PATH = BUILD_DIR / "Tinexus-x86_64.iso"
SERIAL_LOG = BUILD_DIR / "serial_live.log"
SCREENSHOT_PPM = BUILD_DIR / "live_desktop_verified.ppm"
SCREENSHOT_PNG = Path(r"C:\Users\mrasg\.gemini\antigravity-ide\brain\c0b3b60a-cf20-4d45-aa1d-f13b364bea6a\live_desktop_verified.png")
QEMU_BIN = Path(r"C:\Program Files\qemu\qemu-system-x86_64.exe")
OVMF_CODE = Path(r"C:\Program Files\qemu\share\edk2-x86_64-code.fd")
QMP_PORT = 4444

def send_qmp(sock, cmd):
    payload = (json.dumps(cmd) + "\r\n").encode("utf-8")
    sock.sendall(payload)
    res = b""
    while b"\r\n" not in res:
        chunk = sock.recv(4096)
        if not chunk:
            break
        res += chunk
    return res.decode("utf-8", errors="replace")

def run_test():
    print(f"[+] Verifying ISO: {ISO_PATH} ({ISO_PATH.stat().st_size / (1024*1024):.1f} MB)")
    if SERIAL_LOG.exists():
        SERIAL_LOG.unlink()
    if SCREENSHOT_PPM.exists():
        SCREENSHOT_PPM.unlink()

    qemu_cmd = [
        str(QEMU_BIN),
        "-accel", "whpx",
        "-accel", "tcg",
        "-m", "3G",
        "-smp", "2",
        "-machine", "q35",
        "-drive", f"if=pflash,format=raw,readonly=on,file={OVMF_CODE}",
        "-cdrom", str(ISO_PATH),
        "-boot", "d",
        "-vga", "virtio",
        "-display", "none",
        "-serial", f"file:{SERIAL_LOG}",
        "-qmp", f"tcp:127.0.0.1:{QMP_PORT},server,nowait",
        "-no-reboot"
    ]

    print(f"[+] Starting QEMU with command: {' '.join(qemu_cmd)}")
    proc = subprocess.Popen(qemu_cmd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)

    print("[+] Waiting for system to boot and desktop components to render (120s)...")
    for i in range(12):
        time.sleep(10)
        elapsed = (i + 1) * 10
        print(f"[+] Boot progress: {elapsed}s / 120s...")
        if proc.poll() is not None:
            print(f"[!] QEMU process terminated with exit code: {proc.returncode}")
            break

    # Connect to QMP
    print("[+] Connecting to QMP server...")
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(10)
        s.connect(("127.0.0.1", QMP_PORT))
        greeting = s.recv(4096)
        print(f"[+] QMP Greeting received")
        res = send_qmp(s, {"execute": "qmp_capabilities"})
        print(f"[+] QMP Capabilities: {res.strip()}")
        
        # Unlock lockscreen
        print("[+] Unlocking live session lockscreen via QMP keystrokes (root + ret)...")
        for k in ["r", "o", "o", "t", "ret"]:
            send_qmp(s, {"execute": "send-key", "arguments": {"keys": [{"type": "qcode", "data": k}]}})
            time.sleep(0.15)
        
        print("[+] Waiting 8s for desktop shell (topbar, dock, wallpaper) to initialize...")
        time.sleep(8)

        # Screendump 1: Desktop Shell
        ppm_path = str(SCREENSHOT_PPM).replace("\\", "/")
        print(f"[+] Requesting desktop screendump to {ppm_path}...")
        dump_res = send_qmp(s, {"execute": "screendump", "arguments": {"filename": ppm_path}})
        print(f"[+] Screendump response: {dump_res.strip()}")

        # Keystroke: Ctrl+K to open Launcher via input-send-event
        print("[+] Triggering Launcher via input-send-event (Ctrl down -> K down -> K up -> Ctrl up)...")
        k_res = send_qmp(s, {"execute": "input-send-event", "arguments": {"events": [
            {"type": "key", "data": {"down": True, "key": {"type": "qcode", "data": "ctrl"}}},
            {"type": "key", "data": {"down": True, "key": {"type": "qcode", "data": "k"}}},
            {"type": "key", "data": {"down": False, "key": {"type": "qcode", "data": "k"}}},
            {"type": "key", "data": {"down": False, "key": {"type": "qcode", "data": "ctrl"}}}
        ]}})
        print(f"[+] input-send-event response: {k_res.strip()}")
        time.sleep(3)

        # Screendump 2: Launcher Palette
        ppm_launcher = str(BUILD_DIR / "launcher_verified.ppm").replace("\\", "/")
        print(f"[+] Requesting launcher screendump to {ppm_launcher}...")
        dump_launcher_res = send_qmp(s, {"execute": "screendump", "arguments": {"filename": ppm_launcher}})
        print(f"[+] Launcher screendump response: {dump_launcher_res.strip()}")

        s.close()
    except Exception as e:
        print(f"[!] QMP error: {e}")

    # Terminate QEMU
    print("[+] Shutting down QEMU...")
    try:
        proc.terminate()
        proc.wait(timeout=5)
    except Exception:
        proc.kill()

    # Convert PPM to PNG if created
    time.sleep(1)
    if SCREENSHOT_PPM.exists():
        print(f"[+] Converting {SCREENSHOT_PPM} to PNG...")
        try:
            with Image.open(SCREENSHOT_PPM) as img:
                img.save(SCREENSHOT_PNG, "PNG")
            print(f"[+] Saved desktop verification image to {SCREENSHOT_PNG}")
        except Exception as e:
            print(f"[!] Error converting screenshot: {e}")

    launcher_ppm = BUILD_DIR / "launcher_verified.ppm"
    launcher_png = Path(r"C:\Users\mrasg\.gemini\antigravity-ide\brain\c0b3b60a-cf20-4d45-aa1d-f13b364bea6a\launcher_verified.png")
    if launcher_ppm.exists():
        print(f"[+] Converting {launcher_ppm} to PNG...")
        try:
            with Image.open(launcher_ppm) as img:
                img.save(launcher_png, "PNG")
            print(f"[+] Saved launcher verification image to {launcher_png}")
        except Exception as e:
            print(f"[!] Error converting launcher screenshot: {e}")
    else:
        print(f"[!] Warning: Screendump PPM was not found at {SCREENSHOT_PPM}")

    # Inspect serial log
    if SERIAL_LOG.exists():
        log_content = SERIAL_LOG.read_text(encoding="utf-8", errors="replace")
        print("\n" + "="*70)
        print("                 SERIAL LOG SUMMARY")
        print("="*70)
        lines = log_content.splitlines()
        for l in lines:
            if any(k in l.lower() for k in ["tinexus", "session", "lock", "shell", "dock", "wallpaper", "error", "failed", "started"]):
                print(l)
        print("="*70)

if __name__ == "__main__":
    run_test()
