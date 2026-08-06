#!/usr/bin/env bash
set -e

echo "[+] Terminating lingering background daemons..."
pkill -9 -f tinexus-serviced || true
pkill -9 -f tinexus-ipcd || true
pkill -9 -f tinexus-searchd || true
pkill -9 -f tinexus-comp || true

cd /tmp
echo "[+] Syncing code to ~/tinexus/..."
rm -rf ~/tinexus
mkdir -p ~/tinexus/build/debug

cp -r "/mnt/e/Tinu's Technology/Tinexus Manager/src" ~/tinexus/
cp -r "/mnt/e/Tinu's Technology/Tinexus Manager/sdk" ~/tinexus/
cp -r "/mnt/e/Tinu's Technology/Tinexus Manager/protocols" ~/tinexus/
cp -r "/mnt/e/Tinu's Technology/Tinexus Manager/tests" ~/tinexus/
cp -r "/mnt/e/Tinu's Technology/Tinexus Manager/tools" ~/tinexus/
cp -r "/mnt/e/Tinu's Technology/Tinexus Manager/examples" ~/tinexus/ || true
cp -r "/mnt/e/Tinu's Technology/Tinexus Manager/benchmarks" ~/tinexus/ || true
cp -r "/mnt/e/Tinu's Technology/Tinexus Manager/include" ~/tinexus/ || true
cp -r "/mnt/e/Tinu's Technology/Tinexus Manager/Temp" ~/tinexus/ || true
cp "/mnt/e/Tinu's Technology/Tinexus Manager/CMakeLists.txt" ~/tinexus/
cp "/mnt/e/Tinu's Technology/Tinexus Manager/CMakePresets.json" ~/tinexus/
cp "/mnt/e/Tinu's Technology/Tinexus Manager/VERSION" ~/tinexus/

echo "[+] Building all targets in WSL (skipping tests)..."
cd ~/tinexus
cmake --preset debug -DBUILD_TESTING=OFF -B ~/tinexus/build/debug
cmake --build ~/tinexus/build/debug -j2

echo "[+] Build completed successfully."
mkdir -p "/mnt/e/Tinu's Technology/Tinexus Manager/build"
cp -r ~/tinexus/build/debug/bin/* "/mnt/e/Tinu's Technology/Tinexus Manager/build/" || true
cp -r ~/tinexus/build/debug/lib* "/mnt/e/Tinu's Technology/Tinexus Manager/build/" || true
echo "[+] Successfully copied compiled binaries to the workspace build folder!"
