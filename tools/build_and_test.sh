#!/usr/bin/env bash
set -e

echo "[+] Syncing code to ~/tinexus/..."
rm -rf ~/tinexus/build/debug
mkdir -p ~/tinexus/build/debug
cp -r "/mnt/e/Tinu's Technology/Tinexus Manager/src" ~/tinexus/
cp -r "/mnt/e/Tinu's Technology/Tinexus Manager/tests" ~/tinexus/
cp -r "/mnt/e/Tinu's Technology/Tinexus Manager/tools" ~/tinexus/
cp "/mnt/e/Tinu's Technology/Tinexus Manager/CMakeLists.txt" ~/tinexus/
cp "/mnt/e/Tinu's Technology/Tinexus Manager/CMakePresets.json" ~/tinexus/

echo "[+] Building targets in WSL..."
cd ~/tinexus
cmake --preset debug -B ~/tinexus/build/debug
cmake --build ~/tinexus/build/debug --target tinexus-serviced
cmake --build ~/tinexus/build/debug --target tinexusctl
cmake --build ~/tinexus/build/debug --target tinexus-comp
cmake --build ~/tinexus/build/debug --target tinexus-comp-inspector
cmake --build ~/tinexus/build/debug --target tinexus-launcher
cmake --build ~/tinexus/build/debug --target unit_test_common
cmake --build ~/tinexus/build/debug --target unit_test_searchd
cmake --build ~/tinexus/build/debug --target unit_test_serviced
cmake --build ~/tinexus/build/debug --target unit_test_comp
cmake --build ~/tinexus/build/debug --target unit_test_launcher
cmake --build ~/tinexus/build/debug --target integration_test_runtime

echo "[+] Running unit & integration tests..."
ctest --test-dir ~/tinexus/build/debug --output-on-failure
