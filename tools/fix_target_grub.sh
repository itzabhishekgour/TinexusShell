#!/bin/bash
set -euo pipefail

RAW_DISK="/mnt/host/d/tinexus-target.raw"
umount /mnt/efitest 2>/dev/null || true
losetup -D 2>/dev/null || true
sleep 1

LOOP=$(losetup -fP --show "$RAW_DISK")
mkdir -p /mnt/efitest
mount "${LOOP}p1" /mnt/efitest

ROOT_UUID=$(blkid -s UUID -o value "${LOOP}p2")
echo "[+] Root UUID: $ROOT_UUID"

cat > /mnt/efitest/EFI/BOOT/grub.cfg << EOF
set default=0
set timeout=1

insmod part_gpt
insmod ext2

set root=(hd0,2)
search --no-floppy --fs-uuid --set=root $ROOT_UUID

menuentry "Tinexus OS" {
    linux /boot/vmlinuz-7.0.0-31-generic root=UUID=$ROOT_UUID rw quiet splash console=ttyS0,115200n8 console=tty0
    initrd /boot/initrd.img-7.0.0-31-generic
}
EOF

mkdir -p /mnt/efitest/EFI/tinexus
cp -f /mnt/efitest/EFI/BOOT/grub.cfg /mnt/efitest/EFI/tinexus/grub.cfg

echo "[+] Updated /mnt/efitest/EFI/BOOT/grub.cfg:"
cat /mnt/efitest/EFI/BOOT/grub.cfg

umount /mnt/efitest
losetup -d "$LOOP"
echo "[SUCCESS] Target disk bootloader configured."
