#!/usr/bin/env bash
# Gate build + run script — executed by WSL Ubuntu
set -euo pipefail

PROJ_WIN="E:\\Tinu's Technology\\Tinexus Manager"
PROJ="/mnt/e/Tinu's Technology/Tinexus Manager"
BUILD="/tmp/tinexus_gate_build"

echo "=== ENVIRONMENT ==="
echo "WSL distro: $(cat /etc/os-release | grep PRETTY_NAME | cut -d= -f2)"
echo "cmake: $(cmake --version | head -1)"
echo "g++:   $(g++ --version | head -1)"
echo "Qt6:   $(pkg-config --modversion Qt6Core 2>/dev/null || echo NOT_FOUND)"
echo "Qt6WaylandClient: $(pkg-config --exists Qt6WaylandClient && echo YES || echo NO)"
echo "LayerShellQt: $(pkg-config --exists LayerShellQt 2>/dev/null && echo YES || echo NOT_INSTALLED)"

echo ""
echo "=== STEP 1: cmake configure (TIER1_GATE_LAYERSHELL=ON) ==="
rm -rf "$BUILD"
mkdir -p "$BUILD"
cmake -S "$PROJ" -B "$BUILD" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DTIER1_GATE_LAYERSHELL=ON \
  -DTINEXUS_BUILD_QT_APPS=OFF \
  -G Ninja 2>&1 | grep -E "(tier1|Tier|layer|Layer|Gate|Qt6_FOUND|Found Qt|WARNING|Error|CMake Error|Configuring done|-- )" | head -50
echo "configure exit: $?"

echo ""
echo "=== STEP 2: build test-tier1-gate ==="
cmake --build "$BUILD" --target test-tier1-gate -j$(nproc) 2>&1
echo "build exit: $?"

echo ""
echo "=== STEP 3: run --headless ==="
"$BUILD/bin/test-tier1-gate" --headless 2>&1
echo "headless exit: $?"

echo ""
echo "=== STEP 4: run --ipc-messages 5000 (headless) ==="
"$BUILD/bin/test-tier1-gate" --headless --ipc-messages 5000 2>&1
echo "ipc-load exit: $?"
