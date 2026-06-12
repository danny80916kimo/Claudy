#!/usr/bin/env python3
"""
Generates a horizontal sprite sheet template for Claudy.
- 6 frames @ 22x18 each, laid out left-to-right.
- Each slot starts as a faithful 2x upscale of the existing 11x9 idle pose.
- The user paints over frames 1-5 in Aseprite/Piskel.

Frames (left to right):
  0 idle       - eyes open, neutral
  1 blink      - eyes closed
  2 eat_open   - mouth open, food incoming (button A pressed)
  3 eat_chew   - mouth closed, content (after A)
  4 hurt       - recoil, pained expression (button B pressed)
  5 cry        - tears (after B)

Outputs:
  firmware/data/claudy_sheet.png            132x18  (firmware loads this)
  firmware/data/claudy_sheet_preview6x.png  792x108 with frame guides (for humans)
"""

from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

SRC = [
    "           ",
    " ######### ",
    " ######### ",
    " ##.###.## ",
    "###.###.###",
    "###########",
    " ######### ",
    " # #   # # ",
    " # #   # # ",
]

BODY = (204, 120,  92, 255)
EYE  = ( 16,  16,  16, 255)
T    = (  0,   0,   0,   0)

FRAME_W, FRAME_H = 22, 18
SCALE = 2
N_FRAMES = 6
LABELS = ["idle", "blink", "eat_open", "eat_chew", "hurt", "cry"]

SHEET_W = FRAME_W * N_FRAMES   # 132
SHEET_H = FRAME_H              # 18

sheet = Image.new('RGBA', (SHEET_W, SHEET_H), T)
px = sheet.load()

def paint_idle(ox, oy):
    for r in range(9):
        row = SRC[r]
        for c in range(11):
            ch = row[c]
            if   ch == '#': color = BODY
            elif ch == '.': color = EYE
            else:           continue
            for dy in range(SCALE):
                for dx in range(SCALE):
                    px[ox + c * SCALE + dx, oy + r * SCALE + dy] = color

for i in range(N_FRAMES):
    paint_idle(i * FRAME_W, 0)

out_dir = Path(__file__).resolve().parent.parent / 'firmware' / 'data'
out_dir.mkdir(parents=True, exist_ok=True)
sheet.save(out_dir / 'claudy_sheet.png')

# Preview with frame borders + labels (6x).
PV = 6
pv = sheet.resize((SHEET_W * PV, SHEET_H * PV), Image.NEAREST).convert('RGBA')
# Add bottom strip for labels.
label_h = 24
canvas = Image.new('RGBA', (pv.width, pv.height + label_h), (32, 32, 32, 255))
canvas.paste(pv, (0, 0), pv)
draw = ImageDraw.Draw(canvas)
try:
    font = ImageFont.truetype('/System/Library/Fonts/Helvetica.ttc', 14)
except OSError:
    font = ImageFont.load_default()

for i in range(N_FRAMES):
    x0 = i * FRAME_W * PV
    x1 = x0 + FRAME_W * PV - 1
    y1 = SHEET_H * PV - 1
    draw.rectangle([x0, 0, x1, y1], outline=(80, 200, 255, 255), width=1)
    draw.text((x0 + 6, pv.height + 4), f'{i} {LABELS[i]}', fill=(255, 255, 255), font=font)

canvas.save(out_dir / 'claudy_sheet_preview6x.png')

print(f'wrote {out_dir / "claudy_sheet.png"}            ({SHEET_W}x{SHEET_H})')
print(f'wrote {out_dir / "claudy_sheet_preview6x.png"}  ({canvas.width}x{canvas.height})')
