#!/usr/bin/env python3
import socket
import struct
import os
import sys
import time

def get_socket_path():
    uid = os.getuid()
    return f"/run/user/{uid}/tinexus/ipc.sock"

def create_header(msg_type, payload_len):
    # struct Header format (20 bytes):
    # Magic (4) - 0x544E5853 ("TNXS")
    # Version (2) - 0x0100 (v1.0)
    # MessageType (2)
    # Flags (2)
    # SequenceID (4)
    # PayloadLength (4)
    # Checksum (4)
    magic = 0x544E5853
    version = 0x0100
    flags = 0
    seq_id = 1
    checksum = 0 # Optional for now
    
    # '<I H H H I I I' means Little-Endian: uint32, uint16, uint16, uint16, uint32, uint32, uint32
    return struct.pack('<I H H H I I I', magic, version, msg_type, flags, seq_id, payload_len, checksum)

def main():
    sock_path = get_socket_path()
    
    if not os.path.exists(sock_path):
        print(f"[-] Socket not found at {sock_path}")
        print("[-] Make sure tinexus-ipcd is running!")
        sys.exit(1)
        
    print(f"[+] Connecting to IPCD at {sock_path}...")
    
    try:
        client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        client.connect(sock_path)
        print("[+] Connected successfully!")
        
        # Test 1: Send a Ping (SYS_PING = 10)
        print("[+] Sending SYS_PING (Message ID: 10)...")
        header = create_header(10, 0)
        client.sendall(header)
        
        # Wait a bit to let IPCD log it
        time.sleep(1)
        
        # Test 2: Send a Launcher Open (LAUNCHER_OPEN = 1000)
        print("[+] Sending LAUNCHER_OPEN (Message ID: 1000)...")
        header = create_header(1000, 0)
        client.sendall(header)
        
        time.sleep(1)
        
        print("[+] Tests sent! Check the tinexus-ipcd terminal for logs.")
        client.close()
        
    except Exception as e:
        print(f"[-] Error: {e}")

if __name__ == "__main__":
    main()
