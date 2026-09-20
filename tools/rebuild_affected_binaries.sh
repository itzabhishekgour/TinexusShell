#!/bin/bash
set -euo pipefail

echo "================================================================================"
echo "          TINEXUS PLATFORM: REBUILDING AFFECTED CORE BINARIES                   "
echo "================================================================================"

/workspace/tools/ensure_mounts.sh

# 1. Rebuild libtinexus_common.so.0.1.0
echo "[1/4] Rebuilding libtinexus_common.so.0.1.0..."
rm -f /workspace/build/lib/libtinexus_common.so*
BUILD_START_COMMON=$(date +%s)
chroot /mnt/rootfs /usr/bin/g++ -std=c++20 -shared -fPIC \
  -I/workspace/include -I/workspace/src -I/workspace/src/common/include -I/usr/include/libdrm \
  /workspace/src/common/AudioUtils.cpp /workspace/src/common/BacklightUtils.cpp /workspace/src/common/DisplayUtils.cpp \
  /workspace/src/common/GraphicsProbe.cpp /workspace/src/common/HardwareConfig.cpp /workspace/src/common/NetUtils.cpp \
  /workspace/src/common/PlatformServices.cpp /workspace/src/common/TinexusLogo.cpp /workspace/src/common/dbus_power.cpp \
  /workspace/src/common/logger.cpp /workspace/src/common/peer_credentials.cpp /workspace/src/common/string_interner.cpp \
  -ldrm -lgbm -lEGL -lsystemd -lpthread \
  -o /workspace/build/lib/libtinexus_common.so.0.1.0

[ -f /workspace/build/lib/libtinexus_common.so.0.1.0 ] || { echo "libtinexus_common build failed!"; exit 1; }
ln -sf libtinexus_common.so.0.1.0 /workspace/build/lib/libtinexus_common.so
ln -sf libtinexus_common.so.0.1.0 /workspace/build/lib/libtinexus_common.so.0
cp -av /workspace/build/lib/libtinexus_common.so* /mnt/rootfs/usr/lib/x86_64-linux-gnu/ 2>/dev/null || true
cp -av /workspace/build/lib/libtinexus_common.so* /mnt/rootfs/usr/lib/ 2>/dev/null || true
chroot /mnt/rootfs /sbin/ldconfig
echo "[PASS] libtinexus_common.so.0.1.0 built and installed to /mnt/rootfs/usr/lib"

# 2. Rebuild tinexus-serviced
echo "[2/4] Rebuilding tinexus-serviced..."
rm -f /workspace/build/bin/tinexus-serviced
chroot /mnt/rootfs /usr/bin/g++ -std=c++20 -O2 \
  -I/workspace/include -I/workspace/src -I/workspace/src/common/include -I/workspace/src/serviced/include -I/workspace/src/ipcd/include -I/workspace/src/guard/include \
  /workspace/src/serviced/daemon_spec.cpp \
  /workspace/src/serviced/dep_graph.cpp \
  /workspace/src/serviced/event_journal.cpp \
  /workspace/src/serviced/heartbeat_watchdog.cpp \
  /workspace/src/serviced/process_manager.cpp \
  /workspace/src/serviced/runtime_socket.cpp \
  /workspace/src/serviced/launch_authority.cpp \
  /workspace/src/serviced/install_handler.cpp \
  /workspace/src/serviced/logind_mimic.cpp \
  /workspace/src/serviced/main.cpp \
  -L/workspace/build/lib -L/usr/lib -L/usr/lib/x86_64-linux-gnu -ltinexus_common -ltinexus-guard -lcrypto -lsystemd -lpthread \
  -o /workspace/build/bin/tinexus-serviced

[ -f /workspace/build/bin/tinexus-serviced ] || { echo "tinexus-serviced build failed!"; exit 1; }
cp -f /workspace/build/bin/tinexus-serviced /mnt/rootfs/usr/bin/tinexus-serviced
chmod 0755 /mnt/rootfs/usr/bin/tinexus-serviced
echo "[PASS] tinexus-serviced built and installed to /mnt/rootfs/usr/bin/tinexus-serviced ($(ls -lh /workspace/build/bin/tinexus-serviced | awk '{print $5}'))"

# 3. Rebuild tinexus-session
echo "[3/4] Rebuilding tinexus-session..."
rm -f /workspace/build/bin/tinexus-session
chroot /mnt/rootfs /usr/bin/g++ -std=c++20 -O2 \
  -I/workspace/src -I/workspace/src/common/include -I/workspace/src/session/include \
  /workspace/src/session/main.cpp \
  /workspace/src/session/env_bootstrap.cpp \
  /workspace/src/session/autostart_parser.cpp \
  /workspace/src/session/session_manager.cpp \
  -L/workspace/build/lib -L/usr/lib -L/usr/lib/x86_64-linux-gnu -ltinexus_common -lpthread \
  -o /workspace/build/bin/tinexus-session

[ -f /workspace/build/bin/tinexus-session ] || { echo "tinexus-session build failed!"; exit 1; }
cp -f /workspace/build/bin/tinexus-session /mnt/rootfs/usr/bin/tinexus-session
chmod 0755 /mnt/rootfs/usr/bin/tinexus-session
echo "[PASS] tinexus-session built and installed to /mnt/rootfs/usr/bin/tinexus-session ($(ls -lh /workspace/build/bin/tinexus-session | awk '{print $5}'))"

# 4. Rebuild tinexus-comp
echo "[4/4] Rebuilding tinexus-comp..."
rm -f /workspace/build/bin/tinexus-comp
/workspace/tools/compile_comp.sh
[ -f /workspace/build/bin/tinexus-comp ] || { echo "tinexus-comp build failed!"; exit 1; }
cp -f /workspace/build/bin/tinexus-comp /mnt/rootfs/usr/bin/tinexus-comp
chmod 0755 /mnt/rootfs/usr/bin/tinexus-comp
echo "[PASS] tinexus-comp built and installed to /mnt/rootfs/usr/bin/tinexus-comp ($(ls -lh /workspace/build/bin/tinexus-comp | awk '{print $5}'))"

echo "================================================================================"
echo "          ALL AFFECTED CORE BINARIES CLEANLY REBUILT                            "
echo "================================================================================"
