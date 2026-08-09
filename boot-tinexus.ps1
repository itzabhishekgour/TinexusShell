# save as: boot-tinexus.ps1
$root = "E:\Tinu's Technology\Tinexus Manager"
& "C:\Program Files\qemu\qemu-system-x86_64.exe" `
  -m 4096 -machine q35,hpet=off,accel=whpx `
  -drive if=pflash,format=raw,readonly=on,file="$root\build\OVMF_CODE.fd" `
  -drive if=pflash,format=raw,file="$root\build\OVMF_VARS.fd" `
  -cdrom "$root\build\Tinexus-x86_64.iso" `
  -boot d -vga virtio `
  -device virtio-keyboard-pci -device virtio-mouse-pci `
  -display sdl `
  -serial file:"$root\boot_logs.txt"