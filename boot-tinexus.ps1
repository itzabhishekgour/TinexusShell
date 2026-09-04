# boot-tinexus.ps1 — Tinexus OS QEMU Launcher
$root = "E:\Tinu's Technology\Tinexus Manager"

# Kill any existing QEMU instance to release file locks
Get-Process -Name "qemu-system-x86_64" -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 500

# Clear old logs
Remove-Item "$root\boot_logs.txt" -Force -ErrorAction SilentlyContinue
Remove-Item "$root\qemu_out.txt" -Force -ErrorAction SilentlyContinue

& "C:\Program Files\qemu\qemu-system-x86_64.exe" `
  -m 4096 -machine q35,hpet=off,accel=whpx `
  -drive if=pflash,format=raw,readonly=on,file="$root\build\OVMF_CODE.fd" `
  -drive if=pflash,format=raw,file="$root\build\OVMF_VARS.fd" `
  -cdrom "$root\build\Tinexus-x86_64.iso" `
  -boot d -vga virtio `
  -device virtio-keyboard-pci -device virtio-mouse-pci `
  -display sdl `
  -serial file:"$root\boot_logs.txt" 2>&1 | Out-File "$root\qemu_out.txt"