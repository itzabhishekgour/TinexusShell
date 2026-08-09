#!/bin/bash
# tx-appimage: Tinexus AppImage Runner
# Bypasses tinexus-guard to run legacy/third-party AppImages.
# NOTE: This is a Phase 1.5 experimental feature.

APPIMAGE="$1"
if [ -z "$APPIMAGE" ] || [ ! -f "$APPIMAGE" ]; then
    echo "Usage: tx-appimage <path-to-appimage>"
    exit 1
fi
shift

chmod +x "$APPIMAGE"

# 1. Blocking Runtime Warning
echo "============================================================"
echo " WARNING: SECURITY TRADE-OFF"
echo " This app has full access to your system — files, network, and display."
echo " It runs OUTSIDE the Tinexus sandbox."
echo "============================================================"
read -p "Proceed? (y/N) " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Launch aborted by user."
    exit 1
fi

# 2. XDG_RUNTIME_DIR Fallback
if [ -z "$XDG_RUNTIME_DIR" ] || [ ! -d "$XDG_RUNTIME_DIR" ]; then
    export XDG_RUNTIME_DIR="/tmp"
    echo "[tx-appimage] XDG_RUNTIME_DIR missing or invalid. Falling back to /tmp"
fi

# Check if FUSE is available
has_fuse=1
if [ ! -c /dev/fuse ]; then
    has_fuse=0
fi

if ! command -v fusermount3 > /dev/null 2>&1; then
    has_fuse=0
fi

if [ $has_fuse -eq 0 ]; then
    export APPIMAGE_EXTRACT_AND_RUN=1
fi

# 3. OOM Protection (Strict `df` check)
if [ "$APPIMAGE_EXTRACT_AND_RUN" = "1" ]; then
    app_size_kb=$(du -k "$APPIMAGE" | cut -f1)
    req_size_kb=$((app_size_kb * 2))
    avail_kb=$(df -k "$XDG_RUNTIME_DIR" | awk 'NR==2 {print $4}')
    
    if [ -z "$avail_kb" ] || [ "$avail_kb" -lt "$req_size_kb" ]; then
        echo "[tx-appimage] FATAL: Not enough tmpfs space in $XDG_RUNTIME_DIR for extraction."
        echo "Requires at least ${req_size_kb}KB, but only ${avail_kb}KB available."
        echo "Aborting to prevent system OOM crash."
        exit 1
    fi
    echo "[tx-appimage] FUSE unavailable. Using extraction fallback in $XDG_RUNTIME_DIR..."
fi

# 4. Robust Cleanup
if [ "$APPIMAGE_EXTRACT_AND_RUN" = "1" ]; then
    # Create a unique extract dir
    EXTRACT_DIR=$(mktemp -d "$XDG_RUNTIME_DIR/appimage_extract_XXXXXX")
    
    cleanup() {
        echo "[tx-appimage] Cleaning up extracted files in $EXTRACT_DIR..."
        rm -rf "$EXTRACT_DIR"
    }
    
    # Trap SIGINT, SIGTERM, and EXIT
    trap cleanup SIGINT SIGTERM EXIT
    
    echo "[tx-appimage] Extracting payload..."
    # We cd into EXTRACT_DIR so squashfs-root is created there
    cd "$EXTRACT_DIR" || exit 1
    "$APPIMAGE" --appimage-extract > /dev/null
    
    echo "[tx-appimage] Launching extracted payload..."
    "./squashfs-root/AppRun" "$@"
    
else
    echo "[tx-appimage] Launching via FUSE..."
    "$APPIMAGE" "$@"
fi
