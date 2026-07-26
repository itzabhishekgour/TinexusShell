#!/usr/bin/env bash
export PATH="$HOME/tinexus/build/debug/src/serviced:$HOME/tinexus/build/debug/src/ipcd:$HOME/tinexus/build/debug/src/comp:$HOME/tinexus/build/debug/src/launcher:$PATH"

~/tinexus/build/debug/src/serviced/tinexus-serviced &
SERVICED_PID=$!

sleep 1
echo "[+] Querying live status via tinexusctl..."
~/tinexus/build/debug/tools/tinexusctl/tinexusctl status

echo "[+] Querying live health via tinexusctl..."
~/tinexus/build/debug/tools/tinexusctl/tinexusctl health

kill -TERM $SERVICED_PID 2>/dev/null || true
