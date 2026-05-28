# Claudy ESP32-C6 Touch AMOLED 2.16 Port — Design

**Date:** 2026-05-28
**Status:** Draft, awaiting user review
**Target hardware:** Waveshare ESP32-C6-Touch-AMOLED-2.16
**Coexistence:** New firmware lives in `firmware-c6/`; existing `firmware/` (LilyGo T-Display-S3) is untouched.

---

## 1. Background

Claudy is a Claude Code status mascot currently running on the LilyGo T-Display-S3 (ESP32-S3, 170×320 ST7789, 8-bit parallel). Claude Code hook events are POSTed by the Mac bridge to `http://claudy.local/state`, and the device draws a procedural mascot reacting to Idle / Thinking / Working / Waiting / Error / Done plus a token-budget progress bar.

The user wants to also run Claudy on a new board: the **Waveshare ESP32-C6 Touch AMOLED 2.16**. This board is a substantially different hardware platform — a different MCU family, different display driver/bus, larger square display, capacitive touch, and several extra peripherals (PMIC, IMU, RTC, audio codec).

## 2. Goals & Non-Goals

### Goals

- A new firmware tree `firmware-c6/` that runs on the ESP32-C6 AMOLED 2.16 board.
- HTTP API (`POST /state`, `GET /state`, `GET /`), `claudy.local` mDNS hostname, and AppState semantics **stay byte-for-byte identical** — the Mac bridge (`bridge/`) and Claude Code hook integration are unchanged.
- Mascot procedural drawing style is preserved (not replaced with pre-rendered images).
- Minimal touch interaction: tap-to-wake (cancels idle fade, restores brightness).
- Existing T-Display-S3 firmware (`firmware/`) keeps working — no changes there.

### Non-Goals (v1)

- No new touch UI beyond tap-to-wake (no theme switching, no history browsing).
- No use of QMI8658 IMU, PCF85063 RTC, or ES8311 audio in v1.
- No battery / charging UI from AXP2101 in v1 (status read only if trivial).
- No unification of `firmware/` and `firmware-c6/` via `#ifdef` — two parallel trees.

## 3. Hardware Comparison

| Item | T-Display-S3 (existing) | ESP32-C6 AMOLED 2.16 (new) |
|---|---|---|
| MCU | ESP32-S3 dual-core Xtensa LX7 + PSRAM | ESP32-C6 single-core RISC-V @ 160MHz, **no PSRAM** |
| RAM available to app | ~400KB SRAM + 8MB PSRAM | ~400KB SRAM only |
| Flash | 16MB | 16MB |
| Display | 1.9" IPS 170×320 ST7789 | 2.16" AMOLED 480×480 CO5300 |
| Display bus | 8-bit parallel (i80) | QSPI |
| Touch | none | CST9220 capacitive (I2C) |
| WiFi | 2.4GHz WiFi 4 + BLE 5 | WiFi 6 + BLE 5 + Zigbee/Thread |
| Other peripherals | — | AXP2101 PMIC, QMI8658 IMU, PCF85063 RTC, ES8311 audio codec |
| Graphics framework | LovyanGFX (native ST7789 driver) | **LovyanGFX does not support CO5300** → use esp_lcd + LVGL 8 |

The two killer constraints driving every design decision below:

1. **No PSRAM.** A full 480×480×16bpp framebuffer is 460KB and cannot fit in SRAM alongside WiFi/HTTP.
2. **CO5300 has no LovyanGFX driver.** The Waveshare official examples use `esp_lcd_panel_io_spi` + LVGL. We follow that path.

## 4. Architecture

```
Claude Code (Mac)                       Bridge (unchanged)
   └─ hook event JSON                       │
        └─ bridge/send-state.sh             │
             └─ bridge/send_state.py        │
                  └─ POST http://claudy.local/state
                       └─ ESP32-C6 WebServer (Arduino, port 80)
                            └─ AppState (state.h, identical semantics)
                                 └─ LVGL 8 UI
                                      └─ esp_lcd_panel_io_spi → CO5300 (QSPI, DMA)
                            └─ Touch INT (CST9220) → LVGL indev → tap-to-wake
```

Wire-compatible with the existing T-Display-S3 device — the bridge does not know or care which board is on the other end.

## 5. Layout (Display, 480×480)

**Chosen layout: B — upper half mascot, lower half info** (variant of existing landscape layout, rotated 90°).

```
+--------------------------------------------------+
|                                                  |
|                                                  |
|              [ mascot 480 × 240 ]                |   ← Upper half: mascot canvas
|                                                  |     (procedural face, animations)
|                                                  |
|                                                  |
+--------------------------------------------------+
|  Working                                         |   ← State name
|                                                  |
|  [E] Edit  firmware/firmware.ino                 |   ← Tool chip + message (≤ 2 lines, CJK OK)
|                                                  |
|                                                  |
|  ━━━━━━━━━━━━━━░░░░░░░░  42%   85k / 200k        |   ← Token bar + percent + detail
|                                                  |
+--------------------------------------------------+
```

Mascot canvas is **480×240 = 230KB** in RGB565. (Note: this is larger than the 115KB estimated in §1 during brainstorming — see §7 risk; may need to drop to e.g. 360×240 = 172KB if heap budget gets tight.)

## 6. Directory Structure

```
firmware-c6/                       (new, parallel to firmware/)
  firmware-c6.ino                  setup() / loop(); calls lv_timer_handler + server.handleClient
  sketch.yaml                      Arduino CLI profile (esp32c6, 16M Flash, no PSRAM)
  config.h.example
  config.h                         (gitignored)

  hw/
    pins.h                         GPIO defines (LCD QSPI, I2C, Touch)
    co5300.h / co5300.cpp          QSPI bus init, esp_lcd_panel handle, init command sequence
    touch_cst9220.h / .cpp         CST9220 I2C reset + read; GPIO INT routing into LVGL indev
    pmic_axp2101.h / .cpp          AXP2101 init; enable LCD power rail (mandatory before CO5300!)
    lvgl_port.h / .cpp             flush_cb (esp_lcd panel draw), indev_cb (touch), tick source

  ui/
    ui.h / ui.cpp                  Build static screen widgets (state label, tool chip, message, bar)
    mascot.h / mascot.cpp          Procedural drawing into lv_canvas, ported from firmware/mascot.cpp
    gfx.h / gfx.cpp                Thin facade over lv_canvas_draw_* for mascot.cpp
    theme.h                        Color constants (reusing existing RGB565 values), fonts

  net/
    net.h / net.cpp                WiFi + mDNS + WebServer (copied from firmware/net.cpp, includes adjusted)

  state.h                          AppState enums + parsers (copied verbatim from firmware/state.h)
```

`scripts/` additions:
- `build-c6.sh`
- `flash-c6.sh`
- `monitor-c6.sh`

`setup.sh` updated to also install LVGL 8.3.11 + ensure arduino-esp32 ≥ 3.0.7 (required for ESP32-C6 support).

## 7. Memory Strategy

ESP32-C6 has ~400KB usable SRAM after the WiFi stack. The budget must fit three layers:

| Allocation | Size | Where |
|---|---|---|
| LVGL partial draw buffer ×2 (480 × 48 × 2 bytes, double-buffered for DMA) | 92 KB | DRAM |
| Mascot canvas (480 × 240 × 2 bytes RGB565) | 230 KB | DRAM |
| LVGL internal (objects, styles, fonts) | ~30 KB | DRAM |
| WiFi / lwIP / HTTP stack | ~80 KB | DRAM |
| Heap headroom for transient allocations | ≥ 30 KB | DRAM |
| **Total at 480×240 mascot** | ~462 KB | |

**This sum exceeds the ~400KB usable budget — overrun is expected.** This is intentional: we target 480×240 for visual breathing room but **assume we will fall back**. The plan, checked at bring-up gate G5:

1. Boot with 480×240 mascot canvas. Log `ESP.getFreeHeap()` after `ui_init()` returns.
2. If free heap ≥ 30KB headroom → keep 480×240.
3. Else fall back to 400×240 (188KB canvas, saves 42KB) and re-measure.
4. Else fall back to 360×240 (172KB, saves 58KB). The mascot artwork itself fits in 360×240 — the larger canvas is purely margin.

The canvas size is a single `#define` so the fallback is a one-line change + recompile.

**Rendering pipeline:**

- LVGL invalidates only changed regions. Static text/bar areas redraw only on state changes.
- Mascot animations (thinking breathing, working blink) tick every 33ms (~30fps) via `lv_timer`. The mascot module redraws the canvas, marks it invalidated, LVGL flushes through partial buffers.
- DMA pushes partial buffers to CO5300 via QSPI while the next region is being rendered (LVGL double-buffer pattern).

**Idle power saving (existing semantics):**
- 60s with no `/state` POST → fade to Idle state, then reduce LCD brightness via AXP2101 register write.
- Touch INT or new `/state` POST wakes back to full brightness.

## 8. Mascot Porting Detail

`firmware/mascot.cpp` uses 7 LovyanGFX primitives. Map to LVGL 8 canvas:

| LovyanGFX (existing) | LVGL 8 canvas equivalent |
|---|---|
| `canvas.fillScreen(c)` | `lv_canvas_fill_bg(canvas, c, LV_OPA_COVER)` |
| `canvas.fillCircle(x,y,r,c)` | `lv_canvas_draw_arc` with `width = r`, full 0–360° (or set pixels manually for small r) |
| `canvas.fillRect(x,y,w,h,c)` | `lv_canvas_draw_rect` with `rect_dsc.bg_opa = LV_OPA_COVER` |
| `canvas.fillRoundRect(...)` | Same, with `rect_dsc.radius = r` |
| `canvas.drawLine(x0,y0,x1,y1,c)` | `lv_canvas_draw_line(points, 2, &line_dsc)` |
| `canvas.fillEllipse(x,y,rx,ry,c)` | No native; iterate `lv_canvas_set_px` over ellipse equation. Used in mouth/eyes — small regions, cost negligible. |
| `pushRotated` (blink rotation) | LVGL 8 canvas has no rotation. Blink already amounts to vertical squash → reimplement as `scale_y` interpolation in the mascot draw call. |

A thin `gfx.h` facade hides these so `mascot.cpp` itself reads like the existing version:

```c
void gfx_fill_circle(lv_obj_t *canvas, int x, int y, int r, lv_color_t c);
void gfx_fill_rect  (lv_obj_t *canvas, int x, int y, int w, int h, lv_color_t c);
// ... etc.
```

**Color format:** Both old and new use RGB565 16-bit; existing color constants (`0x06DF`, `0xFD20`, `0xF800`, etc.) port directly.

**Animation timing:** Existing mascot uses `millis() % period` for phase. This carries over unchanged; LVGL doesn't interfere — we drive mascot updates from our own `lv_timer` callback at 33ms.

**Fonts:**
- v1: LVGL built-in `lv_font_montserrat_16` / `_24` / `_28` for English + numbers, LVGL built-in CJK font (SimSun 16) for the message line.
- v2 (post-v1): convert existing `efontTW_16` to LVGL via `lv_font_conv` if the built-in CJK glyphs are visually unsatisfactory.

## 9. Hardware Init Sequence

`setup()` order in `firmware-c6.ino`:

1. `Serial.begin(115200)` — debug
2. `Wire.begin(PIN_I2C_SDA=8, PIN_I2C_SCL=7, 400_000)` — shared I2C bus
3. `pmic_init()` — AXP2101: enable LCD power rail. **Required before any CO5300 communication**, otherwise no response on QSPI.
4. `co5300_init()` — QSPI bus configure, esp_lcd_panel_io_spi handle, run init command sequence (lifted from Waveshare reference example).
5. `touch_init()` — CST9220 reset, attach GPIO INT → ISR → notify LVGL indev.
6. `lvgl_port_init()` — register display + indev with LVGL, allocate two 92KB partial draw buffers in DRAM.
7. `ui_init()` — build static UI tree (state label, tool chip placeholder, message label, token bar, mascot canvas).
8. `wifi_begin()` + `mdns_begin("claudy")` + `server.begin()` — networking (ported from `firmware/net.cpp`).
9. Enter `loop()`: `lv_timer_handler()` + `server.handleClient()` + idle-fade check.

### Pin Map (`firmware-c6/hw/pins.h`)

```c
// LCD (CO5300 AMOLED, QSPI)
#define PIN_LCD_CS      15
#define PIN_LCD_SCLK    0
#define PIN_LCD_D0      1
#define PIN_LCD_D1      2
#define PIN_LCD_D2      3
#define PIN_LCD_D3      4
// LCD_RST is not on GPIO — handled by AXP2101 power sequence

// Touch (CST9220, I2C addr 0x5A)
#define PIN_TOUCH_RST   11
#define PIN_TOUCH_INT   5
#define TOUCH_I2C_ADDR  0x5A

// Shared I2C (touch + AXP2101 + QMI8658 + PCF85063)
#define PIN_I2C_SCL     7
#define PIN_I2C_SDA     8
#define I2C_FREQ_HZ     400000

// LCD spec
#define LCD_H_RES       480
#define LCD_V_RES       480
#define LCD_BITS_PP     16   // RGB565
```

All pin values cross-checked against the official Waveshare example at `02_Example/Arduino-v3.3.3/08_LVGL_V8_Test/user_config.h` in the `waveshareteam/ESP32-C6-Touch-AMOLED-2.16` repository.

## 10. Test & Bring-up Plan

No automated unit tests for hardware code. Use sequential bring-up gates — proceed to the next only when the previous passes:

| Gate | What to verify | How |
|---|---|---|
| **G1** I2C bus alive | `0x34` AXP2101, `0x6B` QMI8658, `0x51` PCF85063 respond | I2C scan loop in `setup()` after `Wire.begin` |
| **G2** AXP2101 LCD power | LCD rail high; CST9220 (`0x5A`) now also responds after touch reset | Re-scan I2C after `pmic_init() + touch_init()` |
| **G3** CO5300 raw fill | Display fills solid red / green / blue / white correctly (no byte-order or row-mirror issues) | Send `esp_lcd_panel_draw_bitmap` of a uniform color buffer |
| **G4** LVGL hello | LVGL `lv_label` "Hello" + `lv_obj` colored rect render at correct positions | Minimal LVGL test sketch |
| **G5** Mascot canvas | Idle mascot face appears in upper half, breathing animation runs at ~30fps | Run mascot module standalone with `state = STATE_IDLE` |
| **G6** Touch raw | INT triggers; CST9220 returns sensible X/Y in `[0, 480)` | `Serial.printf` raw coords on tap |
| **G7** WiFi + mDNS | `ping claudy.local` succeeds, `GET /` returns HTML status page | Bridge → curl from Mac |
| **G8** State rendering | All 6 states render correctly, mascot reacts, token bar fills | Run `scripts/test-state.sh` pointed at C6 device IP |
| **G9** End-to-end | Real Claude Code hooks cause C6 device to react in < 200ms | Restart Claude Code, exercise tool use |
| **G10** Soak | 24h continuous run: no crash, no heap leak, idle fade + touch wake both still work | Leave overnight, check `GET /state` uptime + free-heap fields |

## 11. Risks & Mitigations

| Risk | Severity | Mitigation |
|---|---|---|
| SRAM budget overrun (LVGL + 230KB mascot canvas + WiFi simultaneously) | **High** | At G5: log free heap. If < 30KB headroom, shrink mascot canvas to 360×240 (172KB) and center. Layout still works visually. |
| CO5300 init sequence porting (Waveshare reference is ESP-IDF, we want Arduino) | Medium | Don't reinvent: lift the init command array verbatim from the Waveshare example. Validate at G3 with solid color fills before adding LVGL. |
| LVGL 8 canvas missing ellipse + rotation primitives | Low | §8 mapping documented. Ellipse is per-pixel iteration in small regions only. Blink already maps cleanly to scale_y. |
| CJK font quality with LVGL built-in SimSun vs. existing efontTW | Medium | v1 ships with built-in font. Defer custom font conversion to v2 if needed. |
| WiFi 6 / WPA3 compat with older home routers | Low | Arduino WiFi API is identical; fallback `WiFi.setMinSecurity(WIFI_AUTH_WPA2_PSK)` available. |
| Both boards advertising `claudy.local` if user has both online | Low | Single user, single board flashed at a time. Document in `firmware-c6/README.md`. |
| ESP32-C6 USB CDC enumeration on macOS for flashing | Low | Boards present as `/dev/cu.usbmodem*`; `flash-c6.sh` will auto-detect like the existing script. BOOT+RESET fallback documented. |

## 12. Open Questions (Deferred Past v1)

- **Touch UI features:** swipe gestures to switch mascot themes or filter by tool? Long-press to dismiss a `waiting` prompt? All deferred.
- **PCF85063 RTC:** display wall-clock time when in Idle state with no network? Deferred.
- **ES8311 audio + dual mics:** voice confirmation / "OK Claude" wake? Deferred — large scope.
- **AXP2101 battery telemetry:** does this board have a Li-Po connector populated? If yes, show battery % in a corner. To be confirmed from schematic.
- **Unifying firmware trees:** If ESP32-C6 port is successful and matures, consider a third pass to extract a shared `core/` module with HAL abstraction. Not before both have proven stable.

## 13. References

- Waveshare product page: https://www.waveshare.com/esp32-c6-touch-amoled-2.16.htm
- Waveshare wiki / docs: https://docs.waveshare.com/ESP32-C6-Touch-AMOLED-2.16
- Official example code: https://github.com/waveshareteam/ESP32-C6-Touch-AMOLED-2.16
  - Pin definitions: `02_Example/Arduino-v3.3.3/08_LVGL_V8_Test/user_config.h`
  - Touch driver reference: `02_Example/Arduino-v3.3.3/08_LVGL_V8_Test/lcd_touch.cpp`
- Existing Claudy firmware: `firmware/` (this repo, branch `main`)
