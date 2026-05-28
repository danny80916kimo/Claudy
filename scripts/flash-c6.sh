#!/bin/bash
set -euo pipefail
HERE="$(cd "$(dirname "$0")"/.. && pwd)"
SKETCH="$HERE/firmware-c6"

PORT="${1:-}"
if [ -z "$PORT" ]; then
  PORT=$(ls /dev/cu.usbmodem* 2>/dev/null | head -1 || true)
  if [ -z "$PORT" ]; then
    PORT=$(ls /dev/cu.usbserial-* 2>/dev/null | head -1 || true)
  fi
fi
if [ -z "$PORT" ]; then
  echo "No serial port found. Plug in ESP32-C6 (USB-C)."
  echo "If still missing: hold BOOT, press RESET, release BOOT, retry."
  echo "Available ports:"
  ls /dev/cu.* 2>/dev/null || true
  exit 1
fi

FQBN="esp32:esp32:esp32c6:CDCOnBoot=cdc,FlashSize=16M,FlashMode=qio,PartitionScheme=app3M_fat9M_16MB,CPUFreq=160,UploadSpeed=921600,DebugLevel=none"

echo "==> Uploading to $PORT"
arduino-cli upload \
  --fqbn "$FQBN" \
  -p "$PORT" \
  --input-dir "$HERE/build-c6" \
  "$SKETCH"

echo "==> Done. Run ./scripts/monitor-c6.sh to view serial output."
