#!/usr/bin/env bash
set -e

echo "[+] Terminating lingering background daemons..."
pkill -9 -f tinexus-serviced || true
pkill -9 -f tinexus-ipcd || true
pkill -9 -f tinexus-searchd || true
pkill -9 -f tinexus-comp || true

echo "[+] Syncing code to ~/tinexus/..."
mkdir -p ~/tinexus
mkdir -p ~/tinexus/build/debug

rm -rf ~/tinexus/src ~/tinexus/sdk ~/tinexus/protocols ~/tinexus/tests ~/tinexus/tools ~/tinexus/build/debug/*

cp -r "/mnt/e/Tinu's Technology/Tinexus Manager/src" ~/tinexus/
cp -r "/mnt/e/Tinu's Technology/Tinexus Manager/sdk" ~/tinexus/
cp -r "/mnt/e/Tinu's Technology/Tinexus Manager/protocols" ~/tinexus/
cp -r "/mnt/e/Tinu's Technology/Tinexus Manager/tests" ~/tinexus/
cp -r "/mnt/e/Tinu's Technology/Tinexus Manager/tools" ~/tinexus/
cp "/mnt/e/Tinu's Technology/Tinexus Manager/CMakeLists.txt" ~/tinexus/
cp "/mnt/e/Tinu's Technology/Tinexus Manager/CMakePresets.json" ~/tinexus/
cp "/mnt/e/Tinu's Technology/Tinexus Manager/VERSION" ~/tinexus/

echo "[+] Building all targets in WSL..."
cd ~/tinexus
cmake --preset debug -B ~/tinexus/build/debug
cmake --build ~/tinexus/build/debug -j2

echo "[+] Running unit & integration tests..."
ctest --test-dir ~/tinexus/build/debug --output-on-failure
