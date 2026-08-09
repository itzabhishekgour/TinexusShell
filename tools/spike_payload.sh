#!/bin/bash
set -e

echo "=== Tinexus Payload Format Spike ==="

# 1. Setup Dummy Data (~50MB)
mkdir -p /tmp/txapp_spike/payload
dd if=/dev/urandom of=/tmp/txapp_spike/payload/binary.dat bs=1M count=20 2>/dev/null
for i in {1..10}; do
    echo "Creating text file $i..." > /tmp/txapp_spike/payload/text_$i.txt
    head -c 3000000 /dev/zero | tr '\0' 'A' >> /tmp/txapp_spike/payload/text_$i.txt
done

# 2. Measure Disk Size (Compression)
echo "Compressing Tarball (.tar.gz)..."
time tar -czf /tmp/txapp_spike/app.tar.gz -C /tmp/txapp_spike payload

echo "Compressing SquashFS (.squashfs)..."
time mksquashfs /tmp/txapp_spike/payload /tmp/txapp_spike/app.squashfs -comp zstd -noappend > /dev/null

echo ""
echo "=== Disk Usage ==="
ls -lh /tmp/txapp_spike/app.tar.gz /tmp/txapp_spike/app.squashfs

# 3. Measure Latency
echo ""
echo "=== Latency: Tarball Extraction ==="
mkdir -p /tmp/txapp_spike/extract_dest
time tar -xzf /tmp/txapp_spike/app.tar.gz -C /tmp/txapp_spike/extract_dest

echo ""
echo "=== Latency: SquashFS Mount ==="
mkdir -p /tmp/txapp_spike/mount_dest
# We use sudo mount because loop devices require it in standard Linux
echo "Note: Timing sudo mount requires passwordless sudo or will include password prompt time."
# Just to test if we can mount:
if sudo -n mount -o loop -t squashfs /tmp/txapp_spike/app.squashfs /tmp/txapp_spike/mount_dest 2>/dev/null; then
    sudo -n umount /tmp/txapp_spike/mount_dest
    time sudo -n mount -o loop -t squashfs /tmp/txapp_spike/app.squashfs /tmp/txapp_spike/mount_dest
    sudo -n umount /tmp/txapp_spike/mount_dest
else
    echo "Could not sudo mount seamlessly in WSL. We will time squashfuse if available."
    if command -v squashfuse >/dev/null 2>&1; then
        time squashfuse /tmp/txapp_spike/app.squashfs /tmp/txapp_spike/mount_dest
        fusermount -u /tmp/txapp_spike/mount_dest
    else
        echo "squashfuse not installed."
    fi
fi

# Cleanup
rm -rf /tmp/txapp_spike
