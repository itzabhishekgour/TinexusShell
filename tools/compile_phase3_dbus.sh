#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE="$(dirname "$SCRIPT_DIR")"

echo "=== 1. Ensuring Mounts ==="
bash "$WORKSPACE/tools/ensure_mounts.sh"

echo "=== 2. Time Synchronization ==="
hwclock -s 2>/dev/null || true

echo "=== 3. Checking / Installing Build Dependencies in Rootfs ==="
if ! chroot /mnt/rootfs which cmake >/dev/null 2>&1; then
    echo "[INFO] Installing cmake and build tools into rootfs..."
    echo 'nameserver 8.8.8.8' > /mnt/rootfs/etc/resolv.conf
    chroot /mnt/rootfs apt-get update -o Acquire::Check-Valid-Until=false
    chroot /mnt/rootfs apt-get install -y --no-install-recommends \
        cmake make build-essential libsqlite3-dev libvterm-dev
fi

echo "=== 4. Configuring CMake with ENABLE_LEGACY_IPCD=OFF ==="
chroot /mnt/rootfs /bin/bash -c "cd /workspace && cmake -DENABLE_LEGACY_IPCD=OFF -DBUILD_TESTING=OFF -DTINEXUS_ENABLE_SANITIZERS=OFF -B build/debug"

echo "=== 5. Compiling tinexus-settings, tinexus-wallpaper, tinexus-lock ==="
chroot /mnt/rootfs /bin/bash -c "cd /workspace && cmake --build build/debug --target tinexus-settings tinexus-wallpaper tinexus-lock -j4"

echo "=== 6. Build Verification Summary ==="
chroot /mnt/rootfs /bin/bash -c "ls -lh /workspace/build/debug/bin/tinexus-settings /workspace/build/debug/bin/tinexus-wallpaper /workspace/build/debug/bin/tinexus-lock"

echo "[SUCCESS] All Phase 3.5 D-Bus migrated targets compiled successfully with ENABLE_LEGACY_IPCD=OFF!"
