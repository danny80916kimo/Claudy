#!/bin/bash
# One-shot setup for the Claudy firmware toolchain on macOS.
set -euo pipefail

echo "==> Checking Homebrew"
if ! command -v brew >/dev/null; then
  echo "Homebrew not found. Install from https://brew.sh first."
  exit 1
fi

if ! command -v arduino-cli >/dev/null; then
  echo "==> Installing arduino-cli"
  brew install arduino-cli
else
  echo "==> arduino-cli already installed: $(arduino-cli version)"
fi

echo "==> Configuring board manager URL for ESP32"
arduino-cli config init --overwrite >/dev/null 2>&1 || true
arduino-cli config add board_manager.additional_urls \
  https://espressif.github.io/arduino-esp32/package_esp32_index.json 2>/dev/null || true

echo "==> Updating core index"
arduino-cli core update-index

echo "==> Installing esp32:esp32 core (this may take a few minutes)"
arduino-cli core install esp32:esp32

echo "==> Installing libraries"
arduino-cli lib install "LovyanGFX"
arduino-cli lib install "ArduinoJson"

echo "==> Done"
echo "Next:"
echo "  1) cp firmware/config.h.example firmware/config.h  (edit your WiFi)"
echo "  2) ./scripts/build.sh"
echo "  3) Plug in T-Display-S3 then ./scripts/flash.sh"
