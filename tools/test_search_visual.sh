#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE="$(dirname "$SCRIPT_DIR")"
ARTIFACTS_DIR="/mnt/host/c/Users/mrasg/.gemini/antigravity-ide/brain/70aab369-09dc-4076-bf07-d86519e79211"

echo "=== 1. Ensuring Mounts ==="
bash "${WORKSPACE}/tools/ensure_mounts.sh"

echo "=== 2. Rebuilding tinexus-files ==="
chroot /mnt/rootfs /usr/bin/env PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin /usr/bin/bash -c "
    set -euo pipefail
    cd /workspace
    cmake --build build/debug --target tinexus-files -j4
    mkdir -p /usr/share/tinexus/files/qml
    cp -rf /workspace/src/files/qml/* /usr/share/tinexus/files/qml/
"

echo "=== 3. Creating Search Test Fixtures ==="
chroot /mnt/rootfs /usr/bin/env PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin /usr/bin/bash -c '
    set -euo pipefail
    mkdir -p /tmp/test_search_view
    rm -rf /tmp/test_search_view/*
    truncate -s 4M /tmp/test_search_view/report_2026.pdf
    truncate -s 2M /tmp/test_search_view/finance_q3.xlsx
    truncate -s 100K /tmp/test_search_view/notes.txt
    truncate -s 5M /tmp/test_search_view/photo_beach.jpg
    mkdir -p /tmp/test_search_view/Documents
    truncate -s 1M /tmp/test_search_view/Documents/annual_report.pdf
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
  /workspace/build/bin/tinexus-comp > /workspace/build/comp_search_visual.log 2>&1
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

echo "=== 6. Launching tinexus-files with Search Trigger ==="
chroot /mnt/rootfs /usr/bin/env PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin /usr/bin/bash -c "
  export XDG_RUNTIME_DIR=/tmp/tinexus-test-runtime
  export WAYLAND_DISPLAY=wayland-0
  export QT_QPA_PLATFORM=wayland
  export QT_WAYLAND_DISABLE_WINDOWDECORATION=1
  export HOME=/root
  export TINEXUS_FILES_QML=/workspace/src/files/qml/FilesWindow.qml
  export TINEXUS_TRIGGER_TEST_SEARCH=1
  /workspace/build/debug/bin/tinexus-files /tmp/test_search_view > /workspace/build/search_visual.log 2>&1
" &
APP_PID=$!

for s in {1..12}; do
    sleep 1
    echo "[+] Waiting for tinexus-files surface... ($s/12s)"
    if grep -q "New XDG toplevel surface created" /workspace/build/comp_search_visual.log 2>/dev/null; then
        echo "[+] tinexus-files toplevel surface DETECTED by tinexus-comp!"
        break
    fi
done

# Wait for search trigger and UI update
sleep 3

echo "=== 7. Capturing Search View Screendump ==="
echo "screenshot /workspace/build/files_search_view.ppm" > "$FIFO_PATH"
sleep 2

kill $APP_PID $COMP_PID 2>/dev/null || true

if [ -f /mnt/rootfs/workspace/build/files_search_view.ppm ] || [ -f /workspace/build/files_search_view.ppm ]; then
    PPM="/workspace/build/files_search_view.ppm"
    PNG="/workspace/build/files_search_view.png"
    echo "[SUCCESS] Screenshot PPM captured"
    chroot /mnt/rootfs /usr/bin/env PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin /usr/bin/python3 /workspace/tools/ppm_to_png.py "$PPM" "$PNG" 2>/dev/null || true
    if [ -f "$PNG" ]; then
        echo "[SUCCESS] Converted to PNG: $PNG"
        mkdir -p "${ARTIFACTS_DIR}"
        cp -f "$PNG" "${ARTIFACTS_DIR}/files_search_view.png" 2>/dev/null || true
    fi
fi
