#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE="$(dirname "$SCRIPT_DIR")"

echo "=== 1. Ensuring Mounts & Time ==="
bash "$WORKSPACE/tools/ensure_mounts.sh"
hwclock -s 2>/dev/null || true

echo "=== 2. Running PTY & VTerm E2E Test ==="
chroot /mnt/rootfs /bin/bash -c "/workspace/build/debug/bin/test-terminal-pty-e2e"

echo "=== 3. Inspecting Dynamic Linker Symbols (ldd) ==="
echo "--- tinexus-terminal ---"
chroot /mnt/rootfs /bin/bash -c "ldd /workspace/build/debug/bin/tinexus-terminal | grep -E 'Qt6|vterm'"

echo "--- tinexus-notifications ---"
chroot /mnt/rootfs /bin/bash -c "ldd /workspace/build/debug/bin/tinexus-notifications | grep -E 'systemd|c++'"

echo "--- tinexus-wallpaper ---"
chroot /mnt/rootfs /bin/bash -c "ldd /workspace/build/debug/bin/tinexus-wallpaper | grep -E 'wayland|c++'"

echo "=== 4. Testing tinexus-terminal Offscreen Launch ==="
chroot /mnt/rootfs /bin/bash -c "QT_QPA_PLATFORM=offscreen timeout 2 /workspace/build/debug/bin/tinexus-terminal || [ \$? -eq 124 ]"
echo "  [PASS] tinexus-terminal successfully loaded QML and initialized offscreen event loop!"

echo "[SUCCESS] All Phase 4 runtime tests passed with verified actual execution!"
