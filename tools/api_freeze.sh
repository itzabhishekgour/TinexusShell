#!/bin/bash
# Tools for freezing and verifying the libtxui public API for Gate 1

HEADERS=(
    "include/txui/widgets/Widget.hpp"
    "include/txui/window/Window.hpp"
    "include/txui/render/Painter.hpp"
    "include/txui/render/RenderTarget.hpp"
    "include/txui/layout/Constraints.hpp"
    "include/txui/layout/FlexLayout.hpp"
    "include/txui/layout/Padding.hpp"
    "include/txui/core/Event.hpp"
    "include/txui/render/CommandBuffer.hpp"
)

SNAPSHOT_FILE="include/txui/public_api.sha256"

cd "$(dirname "$0")/.."

if [ "$1" == "freeze" ]; then
    echo "Freezing Public API..."
    rm -f "$SNAPSHOT_FILE"
    for header in "${HEADERS[@]}"; do
        if [ -f "$header" ]; then
            sha256sum "$header" >> "$SNAPSHOT_FILE"
        else
            echo "Error: Missing header $header"
            exit 1
        fi
    done
    echo "API Snapshot generated at $SNAPSHOT_FILE"
elif [ "$1" == "verify" ]; then
    echo "Verifying API Snapshot..."
    if [ ! -f "$SNAPSHOT_FILE" ]; then
        echo "FAIL: API snapshot file $SNAPSHOT_FILE not found!"
        exit 1
    fi
    sha256sum --check "$SNAPSHOT_FILE"
    if [ $? -ne 0 ]; then
        echo "FAIL: Public API headers have drifted from frozen state! Engineering Gate 1 violated."
        exit 1
    else
        echo "PASS: Public API matches frozen snapshot."
    fi
else
    echo "Usage: $0 [freeze|verify]"
    exit 1
fi
