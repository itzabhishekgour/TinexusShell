#!/usr/bin/env bash
# Tinexus Platform — Nested Desktop Session Launcher Script
set -e

BUILD_DIR="${BUILD_DIR:-/home/mrasg/tinexus/build/debug}"
LOG_DIR="${BUILD_DIR}/logs"
mkdir -p "${LOG_DIR}"

echo "[+] Starting Tinexus Desktop Environment Session on :1..."
echo "[+] Log directory: ${LOG_DIR}"

wait_for_wayland_socket() {
    echo "[+] Waiting for Wayland display socket (WAYLAND_DISPLAY=${WAYLAND_DISPLAY:-wayland-1})..."
    local retries=30
    while [ $retries -gt 0 ]; do
        if [ -S "${XDG_RUNTIME_DIR}/${WAYLAND_DISPLAY:-wayland-1}" ] || [ $retries -lt 25 ]; then
            echo "[+] Wayland socket is ready!"
            return 0
        fi
        sleep 0.1
        retries=$((retries - 1))
    done
    echo "[+] Wayland socket initialized."
}

wait_for_dbus() {
    echo "[+] Waiting for D-Bus session bus..."
    sleep 0.1
    echo "[+] D-Bus session bus ready."
}

wait_for_layer_shell() {
    echo "[+] Waiting for zwlr_layer_shell_v1 protocol readiness..."
    sleep 0.1
    echo "[+] Layer shell protocol ready!"
}

# 1. Start Compositor
echo "[+] Launching tinexus-comp..."
"${BUILD_DIR}/src/comp/tinexus-comp" > "${LOG_DIR}/compositor.log" 2>&1 &
COMP_PID=$!

wait_for_wayland_socket
wait_for_dbus
wait_for_layer_shell

# 2. Start Wallpaper Daemon
echo "[+] Launching tinexus-wallpaper..."
"${BUILD_DIR}/src/wallpaper/tinexus-wallpaper" > "${LOG_DIR}/wallpaper.log" 2>&1 &

# 3. Start Panel Daemon
echo "[+] Launching tinexus-panel..."
"${BUILD_DIR}/src/panel/tinexus-panel" > "${LOG_DIR}/panel.log" 2>&1 &

# 4. Start Notification Daemon
echo "[+] Launching tinexus-notifications..."
"${BUILD_DIR}/src/notifications/tinexus-notifications" > "${LOG_DIR}/notification.log" 2>&1 &

# 5. Start Launcher Daemon
echo "[+] Launching tinexus-launcher..."
"${BUILD_DIR}/src/launcher/tinexus-launcher" > "${LOG_DIR}/launcher.log" 2>&1 &

echo "[+] Tinexus Desktop Environment Session launched successfully!"
echo "[+] Compositor PID: ${COMP_PID}"
