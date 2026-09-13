import ctypes
from ctypes import wintypes
import subprocess
import json

def test():
    # Verify disk 1 is USB
    cmd = 'powershell -Command "Get-Disk -Number 1 | Select Number, FriendlyName, BusType, Size | ConvertTo-Json"'
    out = subprocess.check_output(cmd, shell=True).decode("utf-8")
    info = json.loads(out)
    print("Disk Info:", info)
    assert info["Number"] == 1
    assert info["BusType"] == 7 or info.get("BusType") == "USB" or "USB" in str(info)
    
    disk_path = r"\\.\PhysicalDrive1"
    handle = ctypes.windll.kernel32.CreateFileW(
        disk_path,
        0x10000000 | 0x40000000 | 0x80000000, # GENERIC_ALL
        0x00000001 | 0x00000002,               # FILE_SHARE_READ | FILE_SHARE_WRITE
        None,
        3,                                      # OPEN_EXISTING
        0,
        None
    )
    err = ctypes.windll.kernel32.GetLastError()
    print("Handle:", handle, "LastError:", err)
    if handle not in (-1, 0):
        ctypes.windll.kernel32.CloseHandle(handle)
        print("Success! Have raw write access to USB drive.")
    else:
        print(f"Failed with Windows error code {err}")

if __name__ == "__main__":
    test()
