#!/bin/bash
# Regenerate the Traditional Chinese LVGL font used by firmware-c6.
#
# Source: PingFangTC-Regular.otf (macOS user font). 24px, 1bpp.
# Coverage: ASCII + General/CJK punctuation + fullwidth + CJK Unified Ideographs
# (0x4E00-0x9FFF) + Compatibility Ideographs — full common Traditional Chinese
# plus punctuation (em-dash, smart quotes, ellipsis) so messages don't box out.
# (CJK Extension A is intentionally excluded to save ~0.3MB flash — those are
#  rare/historical glyphs not seen in everyday messages.)
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
  --range 0x20-0x7F --range 0x2000-0x206F --range 0x3000-0x303F \
  --range 0x4E00-0x9FFF --range 0xF900-0xFAFF --range 0xFF00-0xFFEF \
  --force-fast-kern-format

echo "==> Done. Symbol: font_claudy_cjk24"
