#!/usr/bin/env bash
set -e

TMPDIR=$(mktemp -d)
mkdir -p "$TMPDIR/lib/modules/7.0.0-28-generic"

for m in libarc4 cfg80211 mac80211 mt76 mt76-connac-lib mt792x-lib mt7921-common mt7921e; do
    f=$(find /lib/modules/7.0.0-28-generic -name "${m}.ko*" | head -n 1)
    if [ -n "$f" ]; then
        cp "$f" "$TMPDIR/lib/modules/7.0.0-28-generic/"
        echo "Found $m -> $f"
    else
        echo "NOT FOUND: $m"
    fi
done

for z in "$TMPDIR/lib/modules/7.0.0-28-generic"/*.zst; do
    [ -f "$z" ] && zstd -d --rm "$z" 2>/dev/null || true
done

depmod -b "$TMPDIR" 7.0.0-28-generic 2>&1 || true

echo "=== modules.dep ==="
cat "$TMPDIR/lib/modules/7.0.0-28-generic/modules.dep"

rm -rf "$TMPDIR"
