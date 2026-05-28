#!/bin/bash
set -euo pipefail
PORT="${1:-}"
if [ -z "$PORT" ]; then
  PORT=$(ls /dev/cu.usbmodem* 2>/dev/null | head -1 || true)
fi
if [ -z "$PORT" ]; then
  echo "No serial port found."
  exit 1
fi
echo "==> Monitoring $PORT @ 115200 (Ctrl-A K to quit screen)"
exec screen "$PORT" 115200
