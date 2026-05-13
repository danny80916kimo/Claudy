#include "mascot.h"
#include <Arduino.h>
#include <math.h>

// Claudy pixel-mascot palette
#define CLAUDE_ORANGE 0xCC2D     // ~#CC785C body
#define CLAUDE_DARK   0x1082     // eyes
#define CLAUDE_DIM    0x4208
#define ACCENT_THINK  0x7E5F
#define ACCENT_WORK   0x05FF
#define ACCENT_WAIT   0xFEC0
#define ACCENT_ERROR  0xF820
#define ACCENT_DONE   0x07E5

// 11 cols × 9 rows pixel grid.
// '#' = body pixel, '.' = eye-dark pixel, ' ' = transparent
static const char* CLAUDY_OPEN[] = {
  "           ",   // 0: rounded top
  " ######### ",   // 1
  " ######### ",   // 2
  " ##.###.## ",   // 3: eyes
  "###.###.###",   // 4
  "###########",   // 5
  " #########",   // 6
  " # #   # #  ",   // 7: legs
  " # #   # #  ",   // 8
};
static const char* CLAUDY_BLINK[] = {
  "           ",
  " ######### ",
  " ######### ",
  " ######### ",   // eyes closed — no dark dots
  "###########",
  "###########",
  " ######### ",
  " # #   # # ",
  " # #   # # ",
};
static const char* CLAUDY_SQUINT[] = {
  "           ",
  " ######### ",
  " ##.###.## ",   // eyes one row higher (^ ^)
  " ##.###.## ",
  "###########",
  "###########",
  " ######### ",
  " # #   # # ",
  " # #   # # ",
};
static const char* CLAUDY_X_EYES[] = {
  "           ",
  " ######### ",
  " ##.###.## ",
  " ##.###.## ",   // X-shaped eyes (doubled)
  "###########",
  "###########",
  " #########",
  " # #   # # ",
  " # #   # # ",
};

static const int MASCOT_ROWS = 9;
static const int MASCOT_COLS = 11;

uint32_t mascotAnimInterval(MascotState s) {
  switch (s) {
    case STATE_BOOT:     return 0;
    case STATE_IDLE:     return 120;
    case STATE_THINKING: return 100;
    case STATE_WORKING:  return 80;
    case STATE_WAITING:  return 120;
    case STATE_ERROR:    return 60;
    case STATE_DONE:     return 100;
  }
  return 0;
}

bool mascotAnimating(MascotState s) {
  return mascotAnimInterval(s) > 0;
}

static void drawClaudy(LGFX_Sprite& g, int cx, int cy, int px,
                       const char* const* grid,
                       uint16_t bodyColor, uint16_t eyeColor,
                       int offX, int offY) {
  const int x0 = cx - (MASCOT_COLS * px) / 2 + offX;
  const int y0 = cy - (MASCOT_ROWS * px) / 2 + offY;
  for (int r = 0; r < MASCOT_ROWS; r++) {
    const char* row = grid[r];
    for (int c = 0; c < MASCOT_COLS; c++) {
      char ch = row[c];
      if (ch == '#') {
        g.fillRect(x0 + c * px, y0 + r * px, px, px, bodyColor);
      } else if (ch == '.') {
        g.fillRect(x0 + c * px, y0 + r * px, px, px, eyeColor);
      }
    }
  }
}

void drawMascot(LGFX_Sprite& g, MascotState state, int x, int y, int w, int h) {
  const int cx = x + w / 2;
  const int cy = y + h / 2;

  // Pick the largest pixel size that fits the available area with some margin.
  // 11 cols × 12px = 132 wide; 9 rows × 12px = 108 tall.
  int px = 12;
  if (px * MASCOT_COLS > w) px = w / MASCOT_COLS;
  if (px * MASCOT_ROWS > h) px = h / MASCOT_ROWS;

  unsigned long t = millis();
  int dx = 0, dy = 0;
  uint16_t body = CLAUDE_ORANGE;
  uint16_t eye  = CLAUDE_DARK;
  const char* const* grid = CLAUDY_OPEN;

  switch (state) {
    case STATE_IDLE:
      dy = (int)(sinf((float)t / 1500.0f * 2.0f * (float)PI) * 2.0f);
      if ((t % 3500) < 150) grid = CLAUDY_BLINK;
      break;
    case STATE_THINKING:
      dy = (int)(sinf((float)t / 700.0f * 2.0f * (float)PI) * 1.5f);
      grid = CLAUDY_SQUINT;
      body = ACCENT_THINK;
      break;
    case STATE_WORKING:
      dy = (int)(sinf((float)t / 250.0f * 2.0f * (float)PI) * 1.5f);
      dx = (int)(sinf((float)t / 500.0f * 2.0f * (float)PI) * 1.0f);
      body = ACCENT_WORK;
      break;
    case STATE_WAITING:
      // Slow taller bob, like attentive head-tilt
      dy = (int)(fabsf(sinf((float)t / 900.0f * 2.0f * (float)PI)) * -3.0f);
      body = ACCENT_WAIT;
      break;
    case STATE_ERROR:
      dx = (int)(sinf((float)t / 50.0f * 2.0f * (float)PI) * 4.0f);
      grid = CLAUDY_X_EYES;
      body = ACCENT_ERROR;
      break;
    case STATE_DONE:
      // Excited hop
      dy = (int)(-fabsf(sinf((float)t / 350.0f * 2.0f * (float)PI)) * 5.0f);
      body = ACCENT_DONE;
      if ((t % 700) < 100) grid = CLAUDY_BLINK;   // happy blink
      break;
    case STATE_BOOT:
      body = CLAUDE_DIM;
      break;
  }

  drawClaudy(g, cx, cy, px, grid, body, eye, dx, dy);
}
