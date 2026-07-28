#!/bin/bash
set -euo pipefail
KERNEL_DIR="/mnt/e/Tinu's Technology/Tinexus Manager/build/kernel"
KVER=$(file -b "$KERNEL_DIR/vmlinuz" | grep -o 'version [^ ]*' | awk '{print $2}')
echo "Building Live Initramfs for $KVER..."
dracut --force --no-hostonly --add "dmsquash-live" "$KERNEL_DIR/initramfs.img" "$KVER" 2>/dev/null || true
echo "Success: $(ls -lh "$KERNEL_DIR/initramfs.img")"
