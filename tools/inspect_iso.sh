#!/usr/bin/env bash
ISO="/mnt/e/Tinu's Technology/Tinexus Manager/build/Tinexus-x86_64.iso"
echo "=== file ==="
file "$ISO"
echo ""
echo "=== size ==="
du -sh "$ISO"
echo ""
echo "=== xorriso el torito ==="
xorriso -indev "$ISO" -report_el_torito 2>&1
