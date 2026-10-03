#!/bin/bash
set -e

losetup -D 2>/dev/null || true
sleep 1
LOOP=$(losetup -fP --show /mnt/host/d/tinexus-target.raw)
echo "Loop dev: $LOOP"
mkdir -p /mnt/tgt_esp /mnt/tgt_root
mount "${LOOP}p1" /mnt/tgt_esp
mount "${LOOP}p2" /mnt/tgt_root

ROOT_UUID=$(blkid -s UUID -o value "${LOOP}p2")
echo "Root UUID: $ROOT_UUID"

# Find grub modules in target root
MOD_DIR=$(find /mnt/tgt_root/usr/lib/grub/x86_64-efi -name "ext2.mod" -exec dirname {} \; | head -1)
if [ -n "$MOD_DIR" ]; then
    echo "[+] Found modules at: $MOD_DIR"
    mkdir -p /mnt/tgt_esp/boot/grub/x86_64-efi
    mkdir -p /mnt/tgt_esp/EFI/BOOT/x86_64-efi
    mkdir -p /mnt/tgt_esp/EFI/tinexus/x86_64-efi
    cp -r "$MOD_DIR"/* /mnt/tgt_esp/boot/grub/x86_64-efi/
    cp -r "$MOD_DIR"/* /mnt/tgt_esp/EFI/BOOT/x86_64-efi/
    cp -r "$MOD_DIR"/* /mnt/tgt_esp/EFI/tinexus/x86_64-efi/
    echo "[+] Copied grub modules to ESP boot/grub and EFI/BOOT"
else
    echo "[-] WARNING: ext2.mod not found in /usr/lib/grub/x86_64-efi"
fi

# Check monolithic grub
MONOLITHIC="/mnt/tgt_root/usr/lib/grub/x86_64-efi/monolithic/grubx64.efi"
if [ -f "$MONOLITHIC" ]; then
    echo "[+] Installing monolithic grubx64.efi (has ext2 and gpt compiled-in)"
    cp -f "$MONOLITHIC" /mnt/tgt_esp/EFI/BOOT/grubx64.efi
    cp -f "$MONOLITHIC" /mnt/tgt_esp/EFI/tinexus/grubx64.efi
fi

cat > /mnt/tgt_esp/EFI/BOOT/grub.cfg << EOF
set default=0
set timeout=1

insmod part_gpt
insmod ext2

search --no-floppy --fs-uuid --set=root $ROOT_UUID

menuentry "Tinexus OS" {
    linux /boot/vmlinuz-7.0.0-31-generic root=UUID=$ROOT_UUID rw quiet splash console=ttyS0,115200n8 console=tty0
    initrd /boot/initrd.img-7.0.0-31-generic
}
EOF

cp -f /mnt/tgt_esp/EFI/BOOT/grub.cfg /mnt/tgt_esp/EFI/tinexus/grub.cfg
mkdir -p /mnt/tgt_esp/boot/grub
cp -f /mnt/tgt_esp/EFI/BOOT/grub.cfg /mnt/tgt_esp/boot/grub/grub.cfg
mkdir -p /mnt/tgt_root/boot/grub
cp -f /mnt/tgt_esp/EFI/BOOT/grub.cfg /mnt/tgt_root/boot/grub/grub.cfg

sync
echo "=== ESP structure ==="
find /mnt/tgt_esp -maxdepth 3

umount /mnt/tgt_esp
umount /mnt/tgt_root
losetup -d "$LOOP"
echo "[SUCCESS] ESP fully populated with GRUB modules and config."
