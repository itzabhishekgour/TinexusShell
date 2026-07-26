#!/usr/bin/env bash
set -e

echo "[+] Syncing code to ~/tinexus/..."
rm -rf ~/tinexus/src ~/tinexus/tests ~/tinexus/tools ~/tinexus/CMakeLists.txt ~/tinexus/CMakePresets.json
mkdir -p ~/tinexus
cp -r "/mnt/e/Tinu's Technology/Tinexus Manager/src" ~/tinexus/
cp -r "/mnt/e/Tinu's Technology/Tinexus Manager/tests" ~/tinexus/
cp -r "/mnt/e/Tinu's Technology/Tinexus Manager/tools" ~/tinexus/
cp "/mnt/e/Tinu's Technology/Tinexus Manager/CMakeLists.txt" ~/tinexus/
cp "/mnt/e/Tinu's Technology/Tinexus Manager/CMakePresets.json" ~/tinexus/

echo "[+] Building all targets in WSL..."
cd ~/tinexus
mkdir -p ~/tinexus/build/debug
cmake --preset debug -B ~/tinexus/build/debug
cmake --build ~/tinexus/build/debug -j2

echo "[+] Running unit & integration tests..."
ctest --test-dir ~/tinexus/build/debug --output-on-failure
