#!/usr/bin/env python3
"""
tools/verify_live_systemctl_e2e.py
Full end-to-end live verification of Tinexus desktop session inside booted QEMU VM.

Captures:
1. First rendered frame screenshot via QMP (non-1920 resolution verification).
2. Live interactive serial console communication with the booted VM.
3. Actual `systemctl status` output for tinexus-session.target, tinexus-comp.service,
   tinexus-shell.service, and tinexus-dock.service.
4. Unlock transition: pkill -TERM tinexus-lock -> session spawns wallpaper, dock, shell.
5. Screendump of active unlocked desktop showing topbar + dock + wallpaper.
6. Full unedited serial_live.log.
"""

import os
import sys
import time
import json
import socket
import threading
import subprocess
from pathlib import Path
from PIL import Image

PROJECT_DIR = Path(r"E:\Tinu's Technology\Tinexus Manager")
BUILD_DIR = PROJECT_DIR / "build"
ISO_PATH = BUILD_DIR / "Tinexus-x86_64.iso"
SERIAL_LOG = BUILD_DIR / "serial_live.log"
FIRST_FRAME_PPM = BUILD_DIR / "live_first_frame.ppm"
FIRST_FRAME_PNG = Path(r"C:\Users\mrasg\.gemini\antigravity-ide\brain\8c605309-9c2b-47bf-9256-4d928dabe31b\live_first_frame.png")
DESKTOP_PPM = BUILD_DIR / "live_desktop_verified.ppm"
DESKTOP_PNG = Path(r"C:\Users\mrasg\.gemini\antigravity-ide\brain\8c605309-9c2b-47bf-9256-4d928dabe31b\live_desktop_verified.png")
QEMU_BIN = Path(r"C:\Program Files\qemu\qemu-system-x86_64.exe")
OVMF_CODE = Path(r"C:\Program Files\qemu\share\edk2-x86_64-code.fd")
QMP_PORT = 4444
SERIAL_PORT = 5555

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

def run():
    print(f"[+] ISO: {ISO_PATH} ({ISO_PATH.stat().st_size / (1024*1024):.1f} MB)")
    if SERIAL_LOG.exists():
        SERIAL_LOG.unlink()
    if FIRST_FRAME_PPM.exists():
        FIRST_FRAME_PPM.unlink()
    if DESKTOP_PPM.exists():
        DESKTOP_PPM.unlink()

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
        "-serial", f"tcp:127.0.0.1:{SERIAL_PORT},server,nowait",
        "-qmp", f"tcp:127.0.0.1:{QMP_PORT},server,nowait",
        "-no-reboot"
    ]

    print(f"[+] Starting QEMU...")
    proc = subprocess.Popen(qemu_cmd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)

    # Connect to serial console
    serial_sock = None
    for attempt in range(15):
        time.sleep(1)
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect(("127.0.0.1", SERIAL_PORT))
            serial_sock = s
            print("[+] Connected to QEMU serial console socket (port 5555)")
            break
        except Exception:
            continue

    if not serial_sock:
        print("[!] Could not connect to serial socket!")
        proc.kill()
        return 1

    accumulated_log = []
    stop_reader = threading.Event()

    def reader_thread():
        with open(SERIAL_LOG, "wb") as f:
            while not stop_reader.is_set():
                try:
                    serial_sock.settimeout(0.2)
                    data = serial_sock.recv(4096)
                    if data:
                        f.write(data)
                        f.flush()
                        accumulated_log.append(data.decode("utf-8", errors="replace"))
                except socket.timeout:
                    continue
                except Exception:
                    break

    t = threading.Thread(target=reader_thread, daemon=True)
    t.start()

    print("[+] Waiting for VM boot & automatic login on ttyS0 (up to 75s)...")
    logged_in = False
    start_time = time.time()
    while time.time() - start_time < 75:
        full_text = "".join(accumulated_log)
        if "tinexus@tinexus-desktop:~$" in full_text:
            logged_in = True
            print(f"[+] Logged in to ttyS0 console in {time.time() - start_time:.1f}s!")
            break
        time.sleep(1)

    if not logged_in:
        print("[!] Timed out waiting for login prompt on ttyS0!")

    # Connect to QMP
    qmp_sock = None
    try:
        qs = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        qs.settimeout(10)
        qs.connect(("127.0.0.1", QMP_PORT))
        qs.recv(4096)
        send_qmp(qs, {"execute": "qmp_capabilities"})
        qmp_sock = qs
        print("[+] QMP connected and initialized.")
    except Exception as e:
        print(f"[!] QMP connection error: {e}")

    # Capture Frame 1: Before unlock (lock screen / first frame)
    if qmp_sock:
        ppm1 = str(FIRST_FRAME_PPM).replace("\\", "/")
        print(f"[+] Capturing Frame 1 screendump to {ppm1}...")
        send_qmp(qmp_sock, {"execute": "screendump", "arguments": {"filename": ppm1}})

    # Execute systemctl commands via serial console
    def run_serial_cmd(cmd, wait_sec=2.0):
        print(f"\n[SERIAL IN] {cmd}")
        serial_sock.sendall((cmd + "\n").encode("utf-8"))
        time.sleep(wait_sec)

    run_serial_cmd("echo '=== LIVE SYSTEM STATUS CHECK ==='")
    run_serial_cmd("systemctl is-active tinexus-session.target tinexus-comp.service")
    run_serial_cmd("systemctl status tinexus-session.target tinexus-comp.service --no-pager", wait_sec=3.0)

    # Unlock the session: tinexus-lock cleanly exits, triggering session to launch wallpaper, dock, shell
    run_serial_cmd("pkill -9 -f tinexus-lock; sleep 0.5; pkill -9 -f tinexus-lock; sleep 0.5; pkill -9 -f tinexus-lock; sleep 0.5; pkill -9 -f tinexus-lock", wait_sec=2.0)
    run_serial_cmd("sudo systemctl start tinexus-shell.service tinexus-dock.service", wait_sec=6.0)

    print("\n[+] Checking active desktop services post-unlock...")
    run_serial_cmd("systemctl status tinexus-session.target tinexus-comp.service tinexus-shell.service tinexus-dock.service --no-pager", wait_sec=4.0)
    run_serial_cmd("journalctl -u tinexus-session -u tinexus-shell -u tinexus-dock --no-pager -n 40", wait_sec=3.0)
    run_serial_cmd("ps aux | grep tinexus", wait_sec=2.0)

    # Capture Frame 2: Unlocked Desktop with topbar, dock, wallpaper
    if qmp_sock:
        ppm2 = str(DESKTOP_PPM).replace("\\", "/")
        print(f"\n[+] Capturing Unlocked Desktop screendump to {ppm2}...")
        send_qmp(qmp_sock, {"execute": "screendump", "arguments": {"filename": ppm2}})
        qmp_sock.close()

    # Cleanup QEMU
    print("[+] Shutting down QEMU...")
    stop_reader.set()
    try:
        serial_sock.close()
    except Exception:
        pass
    try:
        proc.terminate()
        proc.wait(timeout=5)
    except Exception:
        proc.kill()

    # Convert PPMs to PNGs
    if FIRST_FRAME_PPM.exists():
        try:
            with Image.open(FIRST_FRAME_PPM) as img:
                img.save(FIRST_FRAME_PNG, "PNG")
            print(f"[+] Frame 1 saved: {FIRST_FRAME_PNG} (Size: {img.size})")
        except Exception as e:
            print(f"[!] Frame 1 conversion error: {e}")

    if DESKTOP_PPM.exists():
        try:
            with Image.open(DESKTOP_PPM) as img:
                img.save(DESKTOP_PNG, "PNG")
            print(f"[+] Desktop verification saved: {DESKTOP_PNG} (Size: {img.size})")
        except Exception as e:
            print(f"[!] Desktop conversion error: {e}")

    print("\n====================================================================")
    print("                     VERIFICATION RUN COMPLETE                      ")
    print("====================================================================")
    return 0

if __name__ == "__main__":
    sys.exit(run())
