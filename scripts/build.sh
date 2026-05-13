#!/bin/bash
set -euo pipefail
HERE="$(cd "$(dirname "$0")"/.. && pwd)"
SKETCH="$HERE/firmware"

if [ ! -f "$SKETCH/config.h" ]; then
  echo "Missing $SKETCH/config.h"
  echo "Run:  cp $SKETCH/config.h.example $SKETCH/config.h  and edit it."
  exit 1
fi

# Stock Arduino-ESP32 doesn't expose a lilygo_t_display_s3 variant, so we
# build for plain esp32s3 with the menu options the board needs.
FQBN="esp32:esp32:esp32s3:USBMode=default,CDCOnBoot=default,FlashSize=16M,FlashMode=qio,PartitionScheme=app3M_fat9M_16MB,LoopCore=1,EventsCore=1,PSRAM=opi,CPUFreq=240,UploadSpeed=921600,DebugLevel=none"

echo "==> Compiling $SKETCH"
arduino-cli compile \
  --fqbn "$FQBN" \
  --build-property "build.extra_flags=-DBOARD_HAS_PSRAM -DARDUINO_USB_CDC_ON_BOOT=1" \
  --output-dir "$HERE/build" \
  "$SKETCH"

echo "==> Built: $HERE/build/firmware.ino.bin"
