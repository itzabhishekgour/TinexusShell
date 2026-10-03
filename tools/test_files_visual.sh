#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE="$(dirname "$SCRIPT_DIR")"
ARTIFACTS_DIR="/mnt/host/c/Users/mrasg/.gemini/antigravity-ide/brain/70aab369-09dc-4076-bf07-d86519e79211"

echo "=== 1. Ensuring Mounts ==="
bash "${WORKSPACE}/tools/ensure_mounts.sh"

echo "=== 1b. Rebuilding tinexus-files with latest main.cpp ==="
chroot /mnt/rootfs /usr/bin/env PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin /usr/bin/bash -c "
    set -euo pipefail
    cd /workspace
    cmake --build build/debug --target tinexus-files -j4
    mkdir -p /usr/share/tinexus/files/qml
    cp -rf /workspace/src/files/qml/* /usr/share/tinexus/files/qml/
"

echo "=== 2. Creating Neutral Filenames Tag Test Fixture Folder ==="
chroot /mnt/rootfs /usr/bin/env PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin /usr/bin/bash -c '
    set -euo pipefail
    mkdir -p /tmp/test_tag_view
    rm -rf /tmp/test_tag_view/*
    touch /tmp/test_tag_view/doc_alpha.pdf
    touch /tmp/test_tag_view/finance_sheet.xlsx
    touch /tmp/test_tag_view/source_code.cpp
    touch /tmp/test_tag_view/vector_art.png
    touch /tmp/test_tag_view/plain_memo.txt

    mkdir -p /root/.config/tinexus
    cat << "EOF" > /root/.config/tinexus/tags.json
{
  "/tmp/test_tag_view/doc_alpha.pdf": "Green",
  "/tmp/test_tag_view/finance_sheet.xlsx": "Purple",
  "/tmp/test_tag_view/source_code.cpp": "Red",
  "/tmp/test_tag_view/vector_art.png": "Blue"
}
EOF
'

echo "=== 3. Setting Up Headless Runtime ==="
mkdir -p /mnt/rootfs/tmp/tinexus-test-runtime
chmod 700 /mnt/rootfs/tmp/tinexus-test-runtime
rm -rf /mnt/rootfs/tmp/tinexus-test-runtime/*

echo "=== 4. Starting tinexus-comp Headless ==="
chroot /mnt/rootfs /usr/bin/env PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin /usr/bin/bash -c "
  export XDG_RUNTIME_DIR=/tmp/tinexus-test-runtime
  export WAYLAND_DISPLAY=wayland-0
  export WLR_BACKENDS=headless
  export WLR_HEADLESS_OUTPUTS=1
  export WLR_LIBINPUT_NO_DEVICES=1
  export WLR_RENDERER=pixman
  /workspace/build/bin/tinexus-comp > /workspace/build/comp_files_visual.log 2>&1
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

echo "=== 5. Launching tinexus-files in Headless Compositor ==="
chroot /mnt/rootfs /usr/bin/env PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin /usr/bin/bash -c "
  export XDG_RUNTIME_DIR=/tmp/tinexus-test-runtime
  export WAYLAND_DISPLAY=wayland-0
  export QT_QPA_PLATFORM=wayland
  export QT_WAYLAND_DISABLE_WINDOWDECORATION=1
  export HOME=/root
  export TINEXUS_FILES_QML=/workspace/src/files/qml/FilesWindow.qml
  /workspace/build/debug/bin/tinexus-files /tmp/test_tag_view > /workspace/build/files_visual.log 2>&1
" &
APP_PID=$!

for s in {1..12}; do
    sleep 1
    echo "[+] Waiting for tinexus-files surface... ($s/12s)"
    if grep -q "New XDG toplevel surface created" /workspace/build/comp_files_visual.log 2>/dev/null; then
        echo "[+] tinexus-files toplevel surface DETECTED by tinexus-comp!"
        break
    fi
done

sleep 3

echo "=== 6. Capturing Screenshot ==="
echo "screenshot /workspace/build/files_tag_dots.ppm" > "$FIFO_PATH"
sleep 2

kill $APP_PID $COMP_PID 2>/dev/null || true

if [ -f /workspace/build/files_tag_dots.ppm ]; then
    echo "[SUCCESS] Screenshot PPM captured ($(stat -c%s /workspace/build/files_tag_dots.ppm) bytes)"
    chroot /mnt/rootfs /usr/bin/env PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin /usr/bin/python3 /workspace/tools/ppm_to_png.py /workspace/build/files_tag_dots.ppm /workspace/build/files_tag_dots.png 2>/dev/null || true
fi
