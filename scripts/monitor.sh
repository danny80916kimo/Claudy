#!/bin/bash
set -euo pipefail

PORT="${1:-}"
if [ -z "$PORT" ]; then
  PORT=$(ls /dev/cu.usbmodem* 2>/dev/null | head -1 || true)
  if [ -z "$PORT" ]; then
    PORT=$(ls /dev/cu.usbserial-* 2>/dev/null | head -1 || true)
  fi
fi
if [ -z "$PORT" ]; then
  echo "No serial port found."
  exit 1
fi

echo "==> Monitoring $PORT  (Ctrl-C to quit)"
arduino-cli monitor -p "$PORT" -c baudrate=115200
