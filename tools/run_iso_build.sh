#!/usr/bin/env bash
set -e

SRC_DIR="/mnt/e/Tinu's Technology/Tinexus Manager"
DST_DIR="$HOME/tinexus"

echo "[+] Syncing source files, tools, and scripts..."
mkdir -p "$DST_DIR"
cp -r "$SRC_DIR/src" "$DST_DIR/"
cp -r "$SRC_DIR/include" "$DST_DIR/"
cp -r "$SRC_DIR/tests" "$DST_DIR/"
cp -r "$SRC_DIR/sdk" "$DST_DIR/"
cp -r "$SRC_DIR/protocols" "$DST_DIR/"
cp -r "$SRC_DIR/tools" "$DST_DIR/"
cp "$SRC_DIR/CMakeLists.txt" "$DST_DIR/"
cp "$SRC_DIR/CMakePresets.json" "$DST_DIR/"
cp "$SRC_DIR/VERSION" "$DST_DIR/"
find "$DST_DIR/src" "$DST_DIR/include" "$DST_DIR/tests" -type f -exec touch {} +

sudo chown -R "$(id -u):$(id -g)" "$DST_DIR/build" 2>/dev/null || true
chmod -R u+w "$DST_DIR/build" 2>/dev/null || true
mkdir -p "$DST_DIR/build/bin" "$DST_DIR/build/lib" "$DST_DIR/build/kernel"
cp -a -f "$SRC_DIR/build/bin/." "$DST_DIR/build/bin/"
cp -a -f "$SRC_DIR/build/lib/." "$DST_DIR/build/lib/" 2>/dev/null || true
cp -a -f "$SRC_DIR/build/kernel/." "$DST_DIR/build/kernel/"
cp -a "$SRC_DIR/assets" "$DST_DIR/" 2>/dev/null || true
cp -a "$SRC_DIR/Temp" "$DST_DIR/" 2>/dev/null || true

cd "$DST_DIR"
echo "[+] Starting Release ISO Build..."
sudo -E PROJECT_DIR="$DST_DIR" bash "$DST_DIR/tools/build_release_iso.sh"

echo "[+] Copying generated ISO back to Windows workspace..."
cp "$DST_DIR/build/Tinexus-x86_64.iso" "$SRC_DIR/build/" || true
cp "$DST_DIR/build/Tinexus-x86_64.iso.sha256" "$SRC_DIR/build/" || true
sudo cp -L "$DST_DIR/build/kernel/vmlinuz" "$SRC_DIR/build/kernel/vmlinuz" 2>/dev/null || true
sudo chown -R "$(id -u):$(id -g)" "$SRC_DIR/build" 2>/dev/null || true

echo "[+] ISO Build Process Complete!"
