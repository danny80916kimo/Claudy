#!/bin/bash
set -euo pipefail
HERE="$(cd "$(dirname "$0")"/.. && pwd)"
SKETCH="$HERE/firmware-c6"

if [ ! -f "$SKETCH/config.h" ]; then
  echo "Missing $SKETCH/config.h"
  echo "Run:  cp $SKETCH/config.h.example $SKETCH/config.h  and edit it."
  exit 1
fi

# Note: USBMode=hwcdc was in the original plan but is not a valid option for
# esp32c6 in arduino-esp32 core 3.0.7/3.3.8 (the C6 only has USB-Serial-JTAG,
# no separate USB peripheral). CDCOnBoot=cdc alone is what's needed.
FQBN="esp32:esp32:esp32c6:CDCOnBoot=cdc,FlashSize=16M,FlashMode=qio,PartitionScheme=app3M_fat9M_16MB,CPUFreq=160,UploadSpeed=921600,DebugLevel=none"

echo "==> Compiling $SKETCH"
arduino-cli compile \
  --fqbn "$FQBN" \
  --output-dir "$HERE/build-c6" \
  "$SKETCH"

echo "==> Built: $HERE/build-c6/firmware-c6.ino.bin"
