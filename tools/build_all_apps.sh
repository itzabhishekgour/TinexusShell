#!/usr/bin/env bash
set -e

SRC_DIR="/mnt/e/Tinu's Technology/Tinexus Manager"
DST_DIR="$HOME/tinexus"

echo "[+] Syncing source files from Windows repo to WSL..."
mkdir -p "$DST_DIR"
cp -r "$SRC_DIR/src" "$DST_DIR/"
cp -r "$SRC_DIR/include" "$DST_DIR/"
cp -r "$SRC_DIR/tests" "$DST_DIR/"
cp -r "$SRC_DIR/sdk" "$DST_DIR/"
cp -r "$SRC_DIR/protocols" "$DST_DIR/"
cp "$SRC_DIR/CMakeLists.txt" "$DST_DIR/"
cp "$SRC_DIR/CMakePresets.json" "$DST_DIR/"
cp "$SRC_DIR/VERSION" "$DST_DIR/"
find "$DST_DIR/src" "$DST_DIR/include" "$DST_DIR/tests" -type f -exec touch {} +

cd "$DST_DIR"
echo "[+] Building all desktop applications..."
cmake --build build/debug --target \
    tinexus-launcher \
    tinexus-store \
    tinexus-lock \
    tinexus-about \
    tinexus-monitor \
    tinexus-files \
    tinexus-terminal \
    tinexus-settings-ui \
    tinexus-notifications \
    tinexus-wallpaper \
    tinexus-comp \
    tinexus-dock \
    tinexus-shell \
    -j2

echo "[+] All desktop applications built successfully!"
