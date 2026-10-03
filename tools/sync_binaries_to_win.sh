#!/usr/bin/env bash
set -euo pipefail

SRC="/home/mrasg/tinexus/build/debug/bin"
WIN="/mnt/e/Tinu's Technology/Tinexus Manager/build/bin"

mkdir -p "$WIN"
cp -p "$SRC/tinexus-shell" "$WIN/"
cp -p "$SRC/tinexus-settings-ui" "$WIN/"
ls -l "$WIN/tinexus-shell" "$WIN/tinexus-settings-ui"
