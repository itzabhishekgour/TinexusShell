#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE="$(dirname "$SCRIPT_DIR")"
ARTIFACTS_DIR="/mnt/host/c/Users/mrasg/.gemini/antigravity-ide/brain/d0d1fb08-bc15-4e60-b76f-51e2393134ef"

echo "=== 1. Ensuring Mounts ==="
bash "${WORKSPACE}/tools/ensure_mounts.sh"

echo "=== 2. Rebuilding tinexus-files ==="
chroot /mnt/rootfs /usr/bin/env PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin /usr/bin/bash -c "
    set -euo pipefail
    cd /workspace
    cmake --build build/debug --target tinexus-files -j\$(nproc)
    mkdir -p /usr/share/tinexus/files/qml
    cp -rf /workspace/src/files/qml/* /usr/share/tinexus/files/qml/
"

echo "=== 3. Creating Tabs Test Fixtures ==="
chroot /mnt/rootfs /usr/bin/env PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin /usr/bin/bash -c '
    set -euo pipefail
    mkdir -p /tmp/test_tabs_view/Downloads
    mkdir -p /tmp/test_tabs_view/Documents
    mkdir -p /tmp/test_tabs_view/Projects

    truncate -s 4M /tmp/test_tabs_view/Downloads/installer_v2.iso
    truncate -s 120K /tmp/test_tabs_view/Downloads/receipt.pdf

    truncate -s 1M /tmp/test_tabs_view/Documents/annual_report.pdf
    truncate -s 450K /tmp/test_tabs_view/Documents/presentation.pptx
    truncate -s 25K /tmp/test_tabs_view/Documents/contract.docx

    truncate -s 80K /tmp/test_tabs_view/Projects/main.cpp
    truncate -s 12K /tmp/test_tabs_view/Projects/CMakeLists.txt
'

echo "=== 4. Setting Up Headless Runtime ==="
mkdir -p /mnt/rootfs/tmp/tinexus-test-runtime
chmod 700 /mnt/rootfs/tmp/tinexus-test-runtime
rm -rf /mnt/rootfs/tmp/tinexus-test-runtime/*

echo "=== 5. Starting tinexus-comp Headless ==="
chroot /mnt/rootfs /usr/bin/env PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin /usr/bin/bash -c "
  export XDG_RUNTIME_DIR=/tmp/tinexus-test-runtime
  export WAYLAND_DISPLAY=wayland-0
  export WLR_BACKENDS=headless
  export WLR_HEADLESS_OUTPUTS=1
  export WLR_LIBINPUT_NO_DEVICES=1
  export WLR_RENDERER=pixman
  /workspace/build/bin/tinexus-comp > /workspace/build/comp_tabs_visual.log 2>&1
" &
COMP_PID=$!

FIFO_PATH="/mnt/rootfs/tmp/tinexus-test-runtime/tinexus/comp-cmd.fifo"

echo "[+] Waiting for compositor socket and FIFO..."
for i in {1..20}; do
    if [ -S /mnt/rootfs/tmp/tinexus-test-runtime/wayland-0 ] && [ -p "$FIFO_PATH" ]; then
        echo "[+] Compositor ready on iteration $i!"
        break
    fi
    sleep 0.5
done

echo "=== 6. Launching tinexus-files with Multi-Tab Trigger ==="
chroot /mnt/rootfs /usr/bin/env PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin /usr/bin/bash -c "
  export XDG_RUNTIME_DIR=/tmp/tinexus-test-runtime
  export WAYLAND_DISPLAY=wayland-0
  export QT_QPA_PLATFORM=wayland
  export QT_WAYLAND_DISABLE_WINDOWDECORATION=1
  export HOME=/root
  export TINEXUS_FILES_QML=/workspace/src/files/qml/FilesWindow.qml
  export TINEXUS_TRIGGER_TEST_TABS=1
  /workspace/build/debug/bin/tinexus-files /tmp/test_tabs_view/Downloads > /workspace/build/tabs_visual.log 2>&1
" &
APP_PID=$!

for s in {1..12}; do
    sleep 1
    echo "[+] Waiting for tinexus-files surface... ($s/12s)"
    if grep -q "New XDG toplevel surface created" /workspace/build/comp_tabs_visual.log 2>/dev/null; then
        echo "[+] tinexus-files toplevel surface DETECTED by tinexus-comp!"
        break
    fi
done

# Wait for tab switching and UI settling
sleep 3

echo "=== 7. Capturing Multi-Tab View Screendump ==="
echo "screenshot /workspace/build/files_tabs_view.ppm" > "$FIFO_PATH"
sleep 2

kill $APP_PID $COMP_PID 2>/dev/null || true

if [ -f /mnt/rootfs/workspace/build/files_tabs_view.ppm ] || [ -f /workspace/build/files_tabs_view.ppm ]; then
    PPM="/workspace/build/files_tabs_view.ppm"
    PNG="/workspace/build/files_tabs_view.png"
    echo "[SUCCESS] Screenshot PPM captured"
    python.exe /workspace/tools/ppm_to_png.py "$PPM" "$PNG" 2>/dev/null || true
    if [ -f "$PNG" ]; then
        echo "[SUCCESS] Converted to PNG: $PNG"
        mkdir -p "${ARTIFACTS_DIR}"
        cp -f "$PNG" "${ARTIFACTS_DIR}/files_tabs_view.png" 2>/dev/null || true
    fi
fi
