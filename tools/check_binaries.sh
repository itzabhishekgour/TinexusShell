#!/usr/bin/env bash
BUILDDIR="/mnt/e/Tinu's Technology/Tinexus Manager/build/debug/src"
for bin in tinexus-comp tinexus-serviced tinexus-searchd tinexus-launcher tinexus-settings tinexus-ipcd; do
    found=$(find "$BUILDDIR" -name "$bin" -type f 2>/dev/null | head -1)
    if [ -n "$found" ]; then
        ftype=$(file -b "$found" | cut -c1-50)
        size=$(du -sh "$found" | cut -f1)
        echo "FOUND  $bin  [$size] $ftype"
    else
        echo "MISSING  $bin"
    fi
done
echo "---"
echo "libtinexus-common.so:"
find "/mnt/e/Tinu's Technology/Tinexus Manager/build/debug" -name "libtinexus*.so*" 2>/dev/null
