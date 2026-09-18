#!/bin/bash
set -eo pipefail

mkdir -p /tmp/wlr_run
chmod 700 /tmp/wlr_run
export XDG_RUNTIME_DIR=/tmp/wlr_run
export WAYLAND_DISPLAY=wayland-0
export WLR_BACKENDS=headless
export WLR_RENDERER=pixman

rm -f /tmp/screencopy_test.png /workspace/screencopy_verified.png

/workspace/build/bin/tinexus-comp > /tmp/comp_headless.log 2>&1 &
COMP_PID=$!

sleep 2

echo "=== RUNNING GRIM SCREENCOPY CAPTURE ==="
grim /tmp/screencopy_test.png
GRIM_STATUS=$?
echo "Grim exit code: $GRIM_STATUS"

if [ -f /tmp/screencopy_test.png ]; then
    echo "=== CAPTURE VERIFICATION SUCCESSFUL ==="
    ls -lh /tmp/screencopy_test.png
    file /tmp/screencopy_test.png
    cp /tmp/screencopy_test.png /workspace/screencopy_verified.png
else
    echo "ERROR: /tmp/screencopy_test.png not found!"
    cat /tmp/comp_headless.log
    kill -9 $COMP_PID 2>/dev/null || true
    rm -rf /tmp/wlr_run
    exit 1
fi

kill -9 $COMP_PID 2>/dev/null || true
rm -rf /tmp/wlr_run
exit $GRIM_STATUS
