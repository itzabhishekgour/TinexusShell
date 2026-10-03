#!/bin/bash
set -euo pipefail

bash /workspace/tools/ensure_mounts.sh

chroot /mnt/rootfs /bin/bash << 'EOF'
set -euo pipefail
rm -rf /tmp/build_settings
mkdir -p /tmp/build_settings

MOC_BIN="/usr/lib/qt6/libexec/moc"
COMMON_INC="-I/workspace/src/common/include -I/workspace/include"
QT6_INC="-I/usr/include/x86_64-linux-gnu/qt6 -I/usr/include/x86_64-linux-gnu/qt6/QtCore -I/usr/include/x86_64-linux-gnu/qt6/QtGui -I/usr/include/x86_64-linux-gnu/qt6/QtQuick -I/usr/include/x86_64-linux-gnu/qt6/QtQml -I/usr/include/x86_64-linux-gnu/qt6/QtNetwork -I/usr/include/x86_64-linux-gnu/qt6/QtDBus"

echo "[INFO] Running moc on settings-ui..."
$MOC_BIN $QT6_INC $COMMON_INC -I/workspace/src/settings-ui/qt /workspace/src/settings-ui/qt/SettingsBridge.hpp -o /tmp/build_settings/moc_SettingsBridge.cpp
$MOC_BIN $QT6_INC $COMMON_INC -I/workspace/src/settings-ui/include/settings-ui /workspace/src/settings-ui/include/settings-ui/SettingsAdaptor.hpp -o /tmp/build_settings/moc_SettingsAdaptor.cpp

for ctl in DisplayController AudioController NetworkController PersonalizationController SystemController AboutController PrivacyController SearchController; do
  $MOC_BIN $QT6_INC $COMMON_INC -I/workspace/src/settings-ui/qt /workspace/src/settings-ui/qt/controllers/${ctl}.hpp -o /tmp/build_settings/moc_${ctl}.cpp
done

echo "[INFO] Compiling tinexus-settings-ui..."
g++ -std=c++20 -O2 \
  $QT6_INC $COMMON_INC \
  -I/workspace/src/guard/include \
  -I/workspace/src/settings-ui \
  -I/workspace/src/settings-ui/qt \
  -I/workspace/src/settings-ui/include \
  -I/tmp/build_settings \
  /workspace/src/settings-ui/qt/main_qt.cpp \
  /workspace/src/settings-ui/qt/SettingsBridge.cpp \
  /workspace/src/settings-ui/qt/SettingsAdaptor.cpp \
  /workspace/src/settings-ui/qt/controllers/*.cpp \
  /workspace/src/settings-ui/WifiManager.cpp \
  /tmp/build_settings/moc_SettingsBridge.cpp \
  /tmp/build_settings/moc_*Controller.cpp \
  -L/workspace/build/lib -L/usr/lib/x86_64-linux-gnu \
  -ltinexus-guard -ltinexus_common -lQt6Core -lQt6Gui -lQt6Quick -lQt6Qml -lQt6Network -lQt6DBus -lcrypto \
  -o /workspace/build/bin/tinexus-settings-ui

ls -lh /workspace/build/bin/tinexus-settings-ui
echo "[SUCCESS] tinexus-settings-ui compiled successfully!"
EOF
