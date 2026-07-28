#!/usr/bin/env bash
set -euo pipefail
PROJ="/mnt/e/Tinu's Technology/Tinexus Manager"
echo "=== Kernel files ==="
ls -lh "$PROJ/build/kernel/"
echo ""
echo "=== vmlinuz type ==="
file "$PROJ/build/kernel/vmlinuz"
echo ""
echo "=== initramfs type ==="
file "$PROJ/build/kernel/initramfs.img"
