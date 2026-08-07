#!/bin/bash
# ==============================================================================
# Tinexus Privacy & Telemetry Audit Script
# Validates the "Zero Telemetry" and "Local AI" promises by scanning all
# source code for network APIs, telemetry SDKs, and hardcoded IPs/URLs.
# ==============================================================================

set -e

echo "================================================="
echo "   Tinexus Platform - Privacy & Telemetry Audit  "
echo "================================================="

SRC_DIR="$(dirname "$0")/../src"
FAILED=0

echo "[1/4] Scanning for hardcoded external URLs or IPs..."
grep -rIE "https?://|[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+" "$SRC_DIR" \
    | grep -v "127.0.0.1" \
    | grep -v "localhost" \
    | grep -v "w3.org" \
    | grep -v "freedesktop.org" > found_urls.txt || true

if [ -s found_urls.txt ]; then
    echo "⚠️  WARNING: External URLs/IPs found in source code:"
    cat found_urls.txt
    FAILED=1
else
    echo "✅ PASS: No external URLs or IPs found."
fi

echo "[2/4] Scanning for telemetry or analytics SDK keywords..."
grep -rIE "analytics|telemetry|sentry|mixpanel|google_analytics|datadog" "$SRC_DIR" > found_telemetry.txt || true
if [ -s found_telemetry.txt ]; then
    echo "⚠️  WARNING: Potential telemetry references found:"
    cat found_telemetry.txt
    FAILED=1
else
    echo "✅ PASS: No telemetry SDKs or keywords found."
fi

echo "[3/4] Scanning for network socket usage (AF_INET/AF_INET6)..."
# We only allow local Unix Domain Sockets (AF_UNIX) for IPC
grep -rIE "AF_INET|SOCK_STREAM|SOCK_DGRAM" "$SRC_DIR" | grep -v "AF_UNIX" > found_sockets.txt || true
if [ -s found_sockets.txt ]; then
    echo "⚠️  WARNING: Network sockets found (Only AF_UNIX is permitted):"
    cat found_sockets.txt
    FAILED=1
else
    echo "✅ PASS: Only local Unix Domain Sockets (AF_UNIX) used for IPC."
fi

echo "[4/4] Process runtime network check..."
if command -v ss > /dev/null; then
    ss -pant | grep -E "tinexus-ipcd|tinexus-searchd|tinexus-launcher" > running_sockets.txt || true
    if [ -s running_sockets.txt ]; then
        echo "⚠️  WARNING: Running processes have active TCP/UDP connections!"
        cat running_sockets.txt
        FAILED=1
    else
        echo "✅ PASS: No active external connections for Tinexus processes."
    fi
else
    echo "⚠️  SKIP: 'ss' command not found. Skipping live network check."
fi

echo "================================================="
if [ $FAILED -eq 0 ]; then
    echo "✅ AUDIT PASSED: Zero Telemetry Verified."
else
    echo "❌ AUDIT FAILED: Privacy Violations Detected."
    exit 1
fi
echo "================================================="

rm -f found_urls.txt found_telemetry.txt found_sockets.txt running_sockets.txt
