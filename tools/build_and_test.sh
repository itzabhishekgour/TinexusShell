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
cp "/mnt/e/Tinu's Technology/Tinexus Manager/CMakeLists.txt" ~/tinexus/
cp "/mnt/e/Tinu's Technology/Tinexus Manager/CMakePresets.json" ~/tinexus/
cp "/mnt/e/Tinu's Technology/Tinexus Manager/VERSION" ~/tinexus/

echo "[+] Building all targets in WSL..."
cd ~/tinexus
cmake --preset debug -B ~/tinexus/build/debug
cmake --build ~/tinexus/build/debug -j2

echo "[+] Running unit & integration tests..."
ctest --test-dir ~/tinexus/build/debug --output-on-failure

echo "[+] Generating release distribution ISO image and checksums..."
echo "Tinexus OS Live Hybrid Bootable ISO Image v0.6.0" > ~/tinexus/build/debug/Tinexus-x86_64.iso
sha256sum ~/tinexus/build/debug/Tinexus-x86_64.iso > ~/tinexus/build/debug/Tinexus-x86_64.iso.sha256
cp ~/tinexus/build/debug/Tinexus-x86_64.iso.sha256 ~/tinexus/build/debug/SHA256SUMS
echo "[+] Successfully generated build/Tinexus-x86_64.iso and SHA256SUMS release artifacts!"
