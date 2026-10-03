#!/usr/bin/env bash
# ============================================================================
# build_release_binaries.sh — Build all Tinexus binaries with CMAKE_BUILD_TYPE=Release
# NDEBUG is enabled, dev-QML guards are disabled, optimized with -O3
# ============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE="$(dirname "$SCRIPT_DIR")"
LOG_FILE="${WORKSPACE}/build/release_build.log"

echo "======================================================================" | tee "${LOG_FILE}"
echo "  Tinexus Platform: Production Release Build (CMAKE_BUILD_TYPE=Release)"| tee -a "${LOG_FILE}"
echo "  $(date -u '+%Y-%m-%dT%H:%M:%SZ')"                                    | tee -a "${LOG_FILE}"
echo "======================================================================" | tee -a "${LOG_FILE}"

bash "${WORKSPACE}/tools/ensure_mounts.sh"

chroot /mnt/rootfs /usr/bin/env PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin /usr/bin/bash -c "
    set -euo pipefail
    cd /workspace
    rm -rf build/release
    mkdir -p build/release
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DENABLE_LEGACY_IPCD=OFF \
        -DBUILD_TESTING=OFF \
        -DTINEXUS_ENABLE_SANITIZERS=OFF \
        -B build/release \
        -S . \
        2>&1
    
    echo '[+] Building tinexus-files in Release mode...'
    cmake --build build/release --target tinexus-files -j\$(nproc) 2>&1
    
    echo '[+] Copying release tinexus-files to /workspace/build/bin/tinexus-files...'
    mkdir -p /workspace/build/bin
    cp -f /workspace/build/release/bin/tinexus-files /workspace/build/bin/tinexus-files
    ls -lh /workspace/build/bin/tinexus-files
" 2>&1 | tee -a "${LOG_FILE}"

echo "[SUCCESS] Release binaries built cleanly." | tee -a "${LOG_FILE}"
