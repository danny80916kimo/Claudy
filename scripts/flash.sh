#!/bin/bash
set -euo pipefail
HERE="$(cd "$(dirname "$0")"/.. && pwd)"
SKETCH="$HERE/firmware"

PORT="${1:-}"
if [ -z "$PORT" ]; then
  # Auto-detect: T-Display-S3 enumerates as /dev/cu.usbmodem* (native USB CDC)
  PORT=$(ls /dev/cu.usbmodem* 2>/dev/null | head -1 || true)
  if [ -z "$PORT" ]; then
    PORT=$(ls /dev/cu.usbserial-* 2>/dev/null | head -1 || true)
  fi
fi
if [ -z "$PORT" ]; then
  echo "No serial port found. Plug in the T-Display-S3 (USB-C)."
  echo "If still missing: hold BOOT, press RESET, release BOOT, retry."
  echo "Available ports:"
  ls /dev/cu.* 2>/dev/null || true
  exit 1
fi

FQBN="esp32:esp32:esp32s3:USBMode=default,CDCOnBoot=default,FlashSize=16M,FlashMode=qio,PartitionScheme=app3M_fat9M_16MB,LoopCore=1,EventsCore=1,PSRAM=opi,CPUFreq=240,UploadSpeed=921600,DebugLevel=none"

echo "==> Uploading to $PORT"
arduino-cli upload \
  --fqbn "$FQBN" \
  -p "$PORT" \
  --input-dir "$HERE/build" \
  "$SKETCH"

echo "==> Done. Run ./scripts/monitor.sh to view serial output."
