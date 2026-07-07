#!/usr/bin/env bash
# fota_upload.sh — Sign and upload firmware to the Jerry FOTA HTTP server.
#
# Usage:
#   ./tools/fota_upload.sh <device-ip> [firmware.bin]
#
# Arguments:
#   device-ip     IP address of the Jerry device (e.g. 192.168.1.100)
#   firmware.bin  Path to the raw NS firmware binary (default: auto-detected
#                 from build/stm-Debug/application/jerry_app.bin)
#
# Prerequisites:
#   - keys/fota_ca.key  (ECDSA-P256 CA private key, gitignored)
#   - keys/fota_ca.crt  (CA certificate, committed)
#   - uv or pip with 'cryptography' package installed
#   - curl
#
# Example:
#   ./tools/fota_upload.sh 192.168.1.100
#   ./tools/fota_upload.sh 192.168.1.100 build/stm-Debug/application/jerry_app.bin

set -euo pipefail

# ---------------------------------------------------------------------------
# Arguments
# ---------------------------------------------------------------------------
if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <device-ip> [firmware.bin]" >&2
    exit 1
fi

DEVICE_IP="$1"
FOTA_PORT=8080
FOTA_URL="http://${DEVICE_IP}:${FOTA_PORT}/fota"

# Auto-detect firmware binary if not provided
if [[ $# -ge 2 ]]; then
    FIRMWARE_BIN="$2"
else
    FIRMWARE_BIN="build/stm-Debug/application/jerry_app.bin"
fi

CA_KEY="keys/fota_ca.key"
CA_CERT="keys/fota_ca.crt"
SIGNED_BIN="${FIRMWARE_BIN%.bin}_signed.bin"

# ---------------------------------------------------------------------------
# Validate inputs
# ---------------------------------------------------------------------------
if [[ ! -f "$FIRMWARE_BIN" ]]; then
    echo "ERROR: Firmware binary not found: $FIRMWARE_BIN" >&2
    echo "       Run 'uv run python tools/build.py build' first." >&2
    exit 1
fi

if [[ ! -f "$CA_KEY" ]]; then
    echo "ERROR: CA private key not found: $CA_KEY" >&2
    echo "       Generate with:" >&2
    echo "         openssl ecparam -name prime256v1 -genkey -noout -out $CA_KEY" >&2
    echo "         openssl req -new -x509 -key $CA_KEY -out $CA_CERT \\" >&2
    echo "             -subj '/CN=Jerry FOTA CA' -days 3650" >&2
    exit 1
fi

if [[ ! -f "$CA_CERT" ]]; then
    echo "ERROR: CA certificate not found: $CA_CERT" >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# Step 1: Sign the firmware
# ---------------------------------------------------------------------------
echo "=== Step 1: Signing firmware ==="
echo "  Input:  $FIRMWARE_BIN ($(wc -c < "$FIRMWARE_BIN") bytes)"
echo "  Output: $SIGNED_BIN"

if command -v uv &>/dev/null; then
    uv run python tools/sign_firmware.py \
        --input  "$FIRMWARE_BIN" \
        --ca-key "$CA_KEY" \
        --ca-cert "$CA_CERT" \
        --output "$SIGNED_BIN"
else
    python3 tools/sign_firmware.py \
        --input  "$FIRMWARE_BIN" \
        --ca-key "$CA_KEY" \
        --ca-cert "$CA_CERT" \
        --output "$SIGNED_BIN"
fi

SIGNED_SIZE=$(wc -c < "$SIGNED_BIN")
echo "  Signed: $SIGNED_BIN ($SIGNED_SIZE bytes)"

# ---------------------------------------------------------------------------
# Step 2: Upload via HTTP POST
# ---------------------------------------------------------------------------
echo ""
echo "=== Step 2: Uploading to $FOTA_URL ==="
echo "  File size: $SIGNED_SIZE bytes"
echo "  This will take ~$(( SIGNED_SIZE / 4096 + 1 )) seconds (flash erase + write)..."
echo ""

# curl flags:
#   -X POST                          — HTTP POST
#   -H "Content-Type: ..."           — binary content type
#   --data-binary @file              — send file as raw binary body
#   --no-progress-meter              — suppress progress bar (cleaner output)
#   --max-time 120                   — 2-minute timeout (erase + write + verify)
#   --retry 0                        — no retries (FOTA is not idempotent)
#   -w "\nHTTP status: %{http_code}\n" — print HTTP status code
#
# Note: curl sends "Expect: 100-continue" by default for large POSTs.
# The Jerry FOTA server handles this correctly (responds 100 Continue).
# To disable: add -H "Expect:" to suppress the Expect header.

HTTP_STATUS=$(curl \
    --silent \
    --show-error \
    --no-progress-meter \
    --max-time 120 \
    --retry 0 \
    -X POST \
    -H "Content-Type: application/octet-stream" \
    --data-binary "@${SIGNED_BIN}" \
    --write-out "%{http_code}" \
    --output /tmp/fota_response.txt \
    "$FOTA_URL" || true)

RESPONSE_BODY=$(cat /tmp/fota_response.txt 2>/dev/null || echo "")

echo "Response: $RESPONSE_BODY"
echo "HTTP status: $HTTP_STATUS"

# ---------------------------------------------------------------------------
# Result
# ---------------------------------------------------------------------------
if [[ "$HTTP_STATUS" == "200" ]]; then
    echo ""
    echo "✅ FOTA upload accepted. Device is rebooting into new firmware."
    echo "   Wait ~5 seconds, then verify the device is running the new version."
else
    echo ""
    echo "❌ FOTA upload failed (HTTP $HTTP_STATUS)."
    echo "   Check device logs for FOTA_ERR_* code."
    exit 1
fi
