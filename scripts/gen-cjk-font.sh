#!/bin/bash
# Regenerate the Traditional Chinese LVGL font used by firmware-c6.
#
# Source: PingFangTC-Regular.otf (macOS user font). 24px, 1bpp.
# Coverage: ASCII + CJK punctuation + fullwidth forms + CJK Unified Ideographs
# (0x4E00-0x9FFF) — full common Traditional Chinese, so messages never box out.
#
# The generated firmware-c6/src/ui/font_claudy_cjk24.c (~9.6MB source, ~1.2MB
# flash) is committed so builds work without this step. Re-run only to change
# the size/bpp/coverage.
#
# Requires: node/npx (lv_font_conv is fetched from npm on first run) and the
# PingFangTC OTF. Swap FONT= for any other CJK ttf/otf/ttc if PingFang is absent.
set -euo pipefail
HERE="$(cd "$(dirname "$0")"/.. && pwd)"
FONT="${FONT:-$HOME/Library/Fonts/PingFangTC-Regular.otf}"
OUT="$HERE/firmware-c6/src/ui/font_claudy_cjk24.c"

if [ ! -f "$FONT" ]; then
  echo "Font not found: $FONT"
  echo "Set FONT=/path/to/a/CJK.(otf|ttf|ttc) and re-run."
  exit 1
fi

echo "==> Generating $OUT from $FONT (this takes ~20s)"
npx -y lv_font_conv@1.5.3 \
  --font "$FONT" --size 24 --bpp 1 \
  --format lvgl --lv-include lvgl.h \
  -o "$OUT" \
  --range 0x20-0x7F --range 0x3000-0x303F --range 0xFF00-0xFFEF --range 0x4E00-0x9FFF \
  --force-fast-kern-format

echo "==> Done. Symbol: font_claudy_cjk24"
