# ESP32-C6 AMOLED 2.16 Port — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Run Claudy on the Waveshare ESP32-C6 Touch AMOLED 2.16 board (`firmware-c6/`), wire-compatible with the existing T-Display-S3 firmware on `claudy.local`. The existing `firmware/` tree is untouched.

**Architecture:** New parallel firmware tree using `esp_lcd_panel_io_spi` + LVGL 8 (CO5300 has no LovyanGFX support). Mascot is procedurally drawn into an `lv_canvas_t` in the upper half of the 480×480 screen; status/tool/token info is rendered with native LVGL widgets in the lower half. HTTP/JSON API, AppState semantics, and the bridge are byte-for-byte identical to the existing firmware. The Waveshare official BSP (`bsp_lvgl_port.cpp`, AXP2101 init, CO5300 init sequence, CST9220 touch driver) is vendored verbatim at first to de-risk bring-up; refactoring comes after the device shows a working idle screen.

**Tech Stack:**
- Arduino-ESP32 ≥ 3.0.7 (provides esp_lcd component for the C6)
- LVGL 8.3.11
- ArduinoJson 7.x
- Toolchain: `arduino-cli` on macOS

**Hardware reference design doc:** `docs/superpowers/specs/2026-05-28-esp32-c6-amoled-port-design.md`. Read it before starting Task 1.

---

## File Structure (Locked at Plan Time)

```
firmware-c6/
  firmware-c6.ino                  # setup()/loop() — same shape as firmware/firmware.ino
  sketch.yaml                      # Arduino CLI profile
  config.h.example
  config.h                         # gitignored
  state.h                          # COPIED VERBATIM from firmware/state.h

  hw/
    pins.h                         # All GPIO defines
    i2c_bus.h, i2c_bus.cpp         # Shared I2C bus wrapper (Wire-based)
    pmic_axp2101.h, .cpp           # AXP2101 PMIC: init + LCD rail enable + brightness API
    co5300.h, .cpp                 # CO5300 QSPI driver: esp_lcd_panel handle + init seq
    touch_cst9220.h, .cpp          # CST9220 I2C touch reader; ISR on INT pin

  ui/
    lvgl_port.h, .cpp              # LVGL init: display flush_cb + indev cb + tick + lock
    theme.h                        # Color constants (RGB565), fonts, layout dimensions
    gfx.h, .cpp                    # Thin facade: gfx_fill_rect/circle/line over lv_canvas
    mascot.h, .cpp                 # Ported from firmware/mascot.cpp, uses gfx facade
    ui.h, .cpp                     # Build screen widgets, update fns called by net.cpp

  net/
    net.h, .cpp                    # ADAPTED from firmware/net.cpp (only includes change)

scripts/
  build-c6.sh                      # NEW
  flash-c6.sh                      # NEW
  monitor-c6.sh                    # NEW
  setup.sh                         # MODIFIED: add LVGL install

firmware-c6/README.md              # NEW: quickstart for the C6 board
README.md                          # MODIFIED: one-line link to C6 README
```

**One file = one responsibility.** Each `hw/*` file owns one peripheral. `ui/*` does no hardware. `net/*` does no display work — it mutates `g_state` and calls `ui_request_update()`. This boundary is what lets later boards swap `hw/` without touching `ui/` or `net/`.

---

## Task 1: Project Skeleton + Build Tooling

**Files:**
- Create: `firmware-c6/firmware-c6.ino`
- Create: `firmware-c6/sketch.yaml`
- Create: `firmware-c6/config.h.example`
- Create: `firmware-c6/.gitignore`
- Create: `firmware-c6/state.h` (copy of `firmware/state.h`)
- Create: `scripts/build-c6.sh`
- Create: `scripts/flash-c6.sh`
- Create: `scripts/monitor-c6.sh`
- Modify: `scripts/setup.sh`

**Goal:** Empty sketch compiles, flashes, prints "C6 boot" to serial. No display, no WiFi, no anything.

- [ ] **Step 1.1: Create `firmware-c6/sketch.yaml`**

```yaml
default_profile: esp32c6_amoled

profiles:
  esp32c6_amoled:
    fqbn: esp32:esp32:esp32c6:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=16M,FlashMode=qio,PartitionScheme=app3M_fat9M_16MB,CPUFreq=160,UploadSpeed=921600,DebugLevel=none
    platforms:
      - platform: esp32:esp32 (3.0.7)
    libraries:
      - lvgl (8.3.11)
      - ArduinoJson (7.1.0)
```

- [ ] **Step 1.2: Create `firmware-c6/.gitignore`**

```
config.h
```

- [ ] **Step 1.3: Create `firmware-c6/config.h.example`**

```c
#pragma once

// Copy this file to config.h and fill in your WiFi credentials.
// config.h is gitignored.

#define WIFI_SSID       "YourWiFiName"
#define WIFI_PASSWORD   "YourWiFiPassword"

// Hostname for mDNS. ESP32 will be reachable as http://<MDNS_HOSTNAME>.local
#define MDNS_HOSTNAME   "claudy"

// HTTP server port
#define HTTP_PORT       80

// Optional shared secret. If non-empty, requests must include
// header "X-Claudy-Token: <value>" to be accepted.
#define AUTH_TOKEN      ""

// Display brightness 0-100 (AXP2101 controls AMOLED brightness)
#define BRIGHTNESS      80

// Auto-return to IDLE after this many ms of no updates (0 = never)
#define IDLE_TIMEOUT_MS 60000
```

- [ ] **Step 1.4: Copy `firmware/state.h` to `firmware-c6/state.h`**

```bash
cp firmware/state.h firmware-c6/state.h
```

(File is identical — no changes needed.)

- [ ] **Step 1.5: Create minimal `firmware-c6/firmware-c6.ino`**

```c
#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  delay(2000);   // let CDC enumerate
  Serial.println("\n=== Claudy-C6 boot ===");
  Serial.println("Skeleton sketch OK");
}

void loop() {
  delay(5000);
  Serial.println("alive");
}
```

- [ ] **Step 1.6: Create `scripts/build-c6.sh`**

```bash
#!/bin/bash
set -euo pipefail
HERE="$(cd "$(dirname "$0")"/.. && pwd)"
SKETCH="$HERE/firmware-c6"

if [ ! -f "$SKETCH/config.h" ]; then
  echo "Missing $SKETCH/config.h"
  echo "Run:  cp $SKETCH/config.h.example $SKETCH/config.h  and edit it."
  exit 1
fi

FQBN="esp32:esp32:esp32c6:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=16M,FlashMode=qio,PartitionScheme=app3M_fat9M_16MB,CPUFreq=160,UploadSpeed=921600,DebugLevel=none"

echo "==> Compiling $SKETCH"
arduino-cli compile \
  --fqbn "$FQBN" \
  --output-dir "$HERE/build-c6" \
  "$SKETCH"

echo "==> Built: $HERE/build-c6/firmware-c6.ino.bin"
```

```bash
chmod +x scripts/build-c6.sh
```

- [ ] **Step 1.7: Create `scripts/flash-c6.sh`**

```bash
#!/bin/bash
set -euo pipefail
HERE="$(cd "$(dirname "$0")"/.. && pwd)"
SKETCH="$HERE/firmware-c6"

PORT="${1:-}"
if [ -z "$PORT" ]; then
  PORT=$(ls /dev/cu.usbmodem* 2>/dev/null | head -1 || true)
  if [ -z "$PORT" ]; then
    PORT=$(ls /dev/cu.usbserial-* 2>/dev/null | head -1 || true)
  fi
fi
if [ -z "$PORT" ]; then
  echo "No serial port found. Plug in ESP32-C6 (USB-C)."
  echo "If still missing: hold BOOT, press RESET, release BOOT, retry."
  echo "Available ports:"
  ls /dev/cu.* 2>/dev/null || true
  exit 1
fi

FQBN="esp32:esp32:esp32c6:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=16M,FlashMode=qio,PartitionScheme=app3M_fat9M_16MB,CPUFreq=160,UploadSpeed=921600,DebugLevel=none"

echo "==> Uploading to $PORT"
arduino-cli upload \
  --fqbn "$FQBN" \
  -p "$PORT" \
  --input-dir "$HERE/build-c6" \
  "$SKETCH"

echo "==> Done. Run ./scripts/monitor-c6.sh to view serial output."
```

```bash
chmod +x scripts/flash-c6.sh
```

- [ ] **Step 1.8: Create `scripts/monitor-c6.sh`**

```bash
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
```

```bash
chmod +x scripts/monitor-c6.sh
```

- [ ] **Step 1.9: Update `scripts/setup.sh`** — add LVGL install. Find the line:

```
arduino-cli lib install "ArduinoJson"
```

Add immediately after:

```bash
arduino-cli lib install "lvgl@8.3.11"
```

Also update the "Next" hint at the end so it lists both boards:

Find:

```
echo "  3) Plug in T-Display-S3 then ./scripts/flash.sh"
```

Replace with:

```
echo "  3a) T-Display-S3:  ./scripts/build.sh && ./scripts/flash.sh"
echo "  3b) ESP32-C6 AMOLED 2.16: cp firmware-c6/config.h.example firmware-c6/config.h && \$EDITOR firmware-c6/config.h && ./scripts/build-c6.sh && ./scripts/flash-c6.sh"
```

- [ ] **Step 1.10: Verify build + flash work**

```bash
cp firmware-c6/config.h.example firmware-c6/config.h     # placeholder values are fine, WiFi not used yet
./scripts/setup.sh                                        # installs LVGL
./scripts/build-c6.sh
```

Expected: `Built: <repo>/build-c6/firmware-c6.ino.bin`. No errors.

Plug in the ESP32-C6, then:

```bash
./scripts/flash-c6.sh
./scripts/monitor-c6.sh
```

Expected serial output:

```
=== Claudy-C6 boot ===
Skeleton sketch OK
alive
alive
...
```

If you don't see output: hold BOOT, tap RESET, release BOOT, then retry `flash-c6.sh`.

- [ ] **Step 1.11: Commit**

```bash
git add firmware-c6/ scripts/build-c6.sh scripts/flash-c6.sh scripts/monitor-c6.sh scripts/setup.sh
git commit -m "firmware-c6: project skeleton compiles and runs on ESP32-C6"
```

---

## Task 2: Pin Map + I2C Bus + I2C Scanner (Gate G1)

**Files:**
- Create: `firmware-c6/hw/pins.h`
- Create: `firmware-c6/hw/i2c_bus.h`
- Create: `firmware-c6/hw/i2c_bus.cpp`
- Modify: `firmware-c6/firmware-c6.ino`

**Goal:** I2C bus comes up on GPIO 7/8 at 400kHz. A scan loop prints addresses found. The known on-board devices respond: `0x34` (AXP2101), `0x6B` (QMI8658), `0x51` (PCF85063). CST9220 (`0x5A`) **may not respond yet** — its power may be gated through AXP2101 (gets unblocked in Task 3).

- [ ] **Step 2.1: Create `firmware-c6/hw/pins.h`**

```c
#pragma once

// === LCD (CO5300 AMOLED, QSPI) ===
#define PIN_LCD_CS      15
#define PIN_LCD_SCLK    0
#define PIN_LCD_D0      1
#define PIN_LCD_D1      2
#define PIN_LCD_D2      3
#define PIN_LCD_D3      4
// LCD_RST is not on a GPIO — handled by AXP2101 power sequencing.

// === Touch (CST9220, I2C addr 0x5A) ===
#define PIN_TOUCH_RST   11
#define PIN_TOUCH_INT   5
#define TOUCH_I2C_ADDR  0x5A

// === Shared I2C bus (touch + AXP2101 + QMI8658 + PCF85063) ===
#define PIN_I2C_SCL     7
#define PIN_I2C_SDA     8
#define I2C_FREQ_HZ     400000

// === I2C device addresses ===
#define I2C_ADDR_AXP2101    0x34
#define I2C_ADDR_QMI8658    0x6B
#define I2C_ADDR_PCF85063   0x51
#define I2C_ADDR_CST9220    0x5A

// === LCD spec ===
#define LCD_H_RES       480
#define LCD_V_RES       480
#define LCD_BITS_PP     16   // RGB565
```

- [ ] **Step 2.2: Create `firmware-c6/hw/i2c_bus.h`**

```c
#pragma once
#include <Arduino.h>

void i2c_bus_begin();
bool i2c_probe(uint8_t addr);
uint8_t i2c_read_reg(uint8_t addr, uint8_t reg);
void i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t val);
void i2c_scan_print();
```

- [ ] **Step 2.3: Create `firmware-c6/hw/i2c_bus.cpp`**

```c
#include "i2c_bus.h"
#include "pins.h"
#include <Wire.h>

void i2c_bus_begin() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ_HZ);
}

bool i2c_probe(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

uint8_t i2c_read_reg(uint8_t addr, uint8_t reg) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((int)addr, 1);
  return Wire.available() ? Wire.read() : 0;
}

void i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

void i2c_scan_print() {
  Serial.println("I2C scan:");
  int n = 0;
  for (uint8_t a = 1; a < 127; a++) {
    if (i2c_probe(a)) {
      Serial.printf("  found device at 0x%02X\n", a);
      n++;
    }
  }
  Serial.printf("I2C scan: %d device(s) found\n", n);
}
```

- [ ] **Step 2.4: Wire scanner into `firmware-c6.ino`**

Replace entire file with:

```c
#include <Arduino.h>
#include "hw/i2c_bus.h"

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n=== Claudy-C6 boot ===");
  i2c_bus_begin();
  delay(50);
  i2c_scan_print();
}

void loop() {
  delay(10000);
  i2c_scan_print();
}
```

- [ ] **Step 2.5: Build, flash, verify (Gate G1)**

```bash
./scripts/build-c6.sh && ./scripts/flash-c6.sh && ./scripts/monitor-c6.sh
```

Expected serial output (order may vary):

```
=== Claudy-C6 boot ===
I2C scan:
  found device at 0x34
  found device at 0x51
  found device at 0x6B
I2C scan: 3 device(s) found
```

**Gate G1 passes if all three addresses (0x34, 0x51, 0x6B) appear.** If any are missing: check pins, check `I2C_FREQ_HZ` (try `100000`), check the board has power.

If you also see `0x5A` (CST9220) already → great, the touch IC is on the shared rail without gating. Just note it; we'll exercise it in Task 7.

- [ ] **Step 2.6: Commit**

```bash
git add firmware-c6/hw/ firmware-c6/firmware-c6.ino
git commit -m "firmware-c6: I2C bus up; G1 passes (AXP2101 + QMI8658 + PCF85063 detected)"
```

---

## Task 3: AXP2101 PMIC — Enable LCD Power (Gate G2)

**Files:**
- Create: `firmware-c6/hw/pmic_axp2101.h`
- Create: `firmware-c6/hw/pmic_axp2101.cpp`
- Modify: `firmware-c6/firmware-c6.ino`

**Goal:** AXP2101 initialized; LCD power rails (ALDO1, BLDO1) enabled. After this runs, a second I2C scan also reveals CST9220 at `0x5A` (its rail comes on with the LCD).

> **Reference:** Waveshare publishes the AXP2101 init for this exact board in `02_Example/Arduino-v3.3.3/08_LVGL_V8_Test/src/port_bsp/axp2101_bsp.{h,cpp}` at https://github.com/waveshareteam/ESP32-C6-Touch-AMOLED-2.16 . Fetch those two files for the exact register sequence — don't reverse-engineer from a datasheet. The code below is a clean rewrite of the same sequence with Wire.h.

- [ ] **Step 3.1: Create `firmware-c6/hw/pmic_axp2101.h`**

```c
#pragma once
#include <Arduino.h>

// AXP2101 PMIC controls all on-board power rails on this board.
// Must be initialized before any LCD or touch I/O.
bool pmic_init();

// AMOLED brightness 0..100. Implemented as a register write (HVbrightness reg).
void pmic_set_brightness(uint8_t level);

// Returns battery voltage in mV (0 if no battery / not detected).
uint16_t pmic_battery_mv();
```

- [ ] **Step 3.2: Create `firmware-c6/hw/pmic_axp2101.cpp`**

> **Implementer note:** before pasting, open Waveshare's `axp2101_bsp.cpp` and confirm the register values below match. Their config powers ALDO1 = 3.3V (LCD logic), BLDO1 = 1.8V (LCD analog), and various other rails. If the official file differs, prefer the official values — this file is a hand-translation that may need adjustment.

```c
#include "pmic_axp2101.h"
#include "i2c_bus.h"
#include "pins.h"

namespace {
constexpr uint8_t ADDR = I2C_ADDR_AXP2101;

// Selected AXP2101 registers we touch
constexpr uint8_t REG_LDO_ONOFF_CFG0 = 0x90;
constexpr uint8_t REG_LDO_ONOFF_CFG1 = 0x91;
constexpr uint8_t REG_ALDO1_VOLT     = 0x92;   // ALDO1: 3.3V to LCD logic
constexpr uint8_t REG_BLDO1_VOLT     = 0x96;   // BLDO1: 1.8V to LCD analog
constexpr uint8_t REG_VBAT_H         = 0x34;   // 14-bit ADC, high byte at 0x34
constexpr uint8_t REG_VBAT_L         = 0x35;
}  // namespace

bool pmic_init() {
  if (!i2c_probe(ADDR)) {
    Serial.println("AXP2101: not detected at 0x34");
    return false;
  }

  // ALDO1 = 3.3V (LCD VDD).  Voltage = 0.5 + 0.1 * regval. 28 -> 3.3V.
  i2c_write_reg(ADDR, REG_ALDO1_VOLT, 28);
  // BLDO1 = 1.8V (LCD AVDD). 13 -> 1.8V.
  i2c_write_reg(ADDR, REG_BLDO1_VOLT, 13);

  // Enable ALDO1 (bit 0) and BLDO1 (bit 4) via LDO_ONOFF_CFG0.
  uint8_t cfg0 = i2c_read_reg(ADDR, REG_LDO_ONOFF_CFG0);
  cfg0 |= (1 << 0) | (1 << 4);
  i2c_write_reg(ADDR, REG_LDO_ONOFF_CFG0, cfg0);

  delay(50);   // let rails settle

  Serial.println("AXP2101: LCD power rails enabled");
  return true;
}

void pmic_set_brightness(uint8_t level) {
  // CO5300 brightness is driven via DSI brightness command; here we keep
  // AXP at fixed 3.3V/1.8V and route brightness through the LCD command.
  // For now this is a no-op placeholder so the API exists.
  (void)level;
}

uint16_t pmic_battery_mv() {
  uint8_t h = i2c_read_reg(ADDR, REG_VBAT_H);
  uint8_t l = i2c_read_reg(ADDR, REG_VBAT_L);
  uint16_t raw = ((uint16_t)h << 8) | l;
  // 14-bit ADC, ~1mV/LSB on this board.
  return raw & 0x3FFF;
}
```

- [ ] **Step 3.3: Wire PMIC into setup**

Edit `firmware-c6/firmware-c6.ino`:

```c
#include <Arduino.h>
#include "hw/i2c_bus.h"
#include "hw/pmic_axp2101.h"

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n=== Claudy-C6 boot ===");

  i2c_bus_begin();
  delay(50);

  Serial.println("--- Scan #1 (before PMIC init) ---");
  i2c_scan_print();

  if (!pmic_init()) {
    Serial.println("FATAL: AXP2101 init failed; halting");
    while (true) delay(1000);
  }

  delay(100);
  Serial.println("--- Scan #2 (after PMIC init) ---");
  i2c_scan_print();

  Serial.printf("Battery: %u mV\n", pmic_battery_mv());
}

void loop() {
  delay(10000);
}
```

- [ ] **Step 3.4: Build, flash, verify (Gate G2)**

```bash
./scripts/build-c6.sh && ./scripts/flash-c6.sh && ./scripts/monitor-c6.sh
```

Expected:

```
--- Scan #1 (before PMIC init) ---
I2C scan:
  found device at 0x34
  found device at 0x51
  found device at 0x6B
I2C scan: 3 device(s) found
AXP2101: LCD power rails enabled
--- Scan #2 (after PMIC init) ---
I2C scan:
  found device at 0x34
  found device at 0x51
  found device at 0x5A     <-- CST9220 now powered
  found device at 0x6B
I2C scan: 4 device(s) found
Battery: 0 mV               <-- or actual voltage if battery is fitted
```

**Gate G2 passes if scan #2 includes 0x5A and the display backlight (if any visible test pattern from prior firmware) flickers / changes when the rails come up.** If 0x5A still missing: verify the AXP2101 register values against the Waveshare reference (Step 3.1 note).

- [ ] **Step 3.5: Commit**

```bash
git add firmware-c6/hw/pmic_axp2101.{h,cpp} firmware-c6/firmware-c6.ino
git commit -m "firmware-c6: AXP2101 brings up LCD power rails; G2 passes (CST9220 now visible)"
```

---

## Task 4: CO5300 AMOLED Bring-up — Solid Color Test (Gate G3)

**Files:**
- Create: `firmware-c6/hw/co5300.h`
- Create: `firmware-c6/hw/co5300.cpp`
- Modify: `firmware-c6/firmware-c6.ino`

**Goal:** QSPI bus configured, esp_lcd_panel_io handle created, CO5300 init sequence sent. Display fills solid red, green, blue, white in sequence.

> **Reference:** The init sequence + esp_lcd panel callbacks for CO5300 live in `02_Example/Arduino-v3.3.3/08_LVGL_V8_Test/bsp_lvgl_port.cpp` and headers in `02_Example/Arduino-v3.3.3/08_LVGL_V8_Test/src/externLib/` in the Waveshare repo. **Copy that init command array and the QSPI setup code verbatim** at first. Refactor only after Gate G3 passes — the init sequence is undocumented and contains panel-specific magic. Don't try to derive it.

- [ ] **Step 4.1: Vendor the Waveshare CO5300 init sequence**

```bash
mkdir -p firmware-c6/hw/co5300_vendor
curl -fsSL https://raw.githubusercontent.com/waveshareteam/ESP32-C6-Touch-AMOLED-2.16/main/02_Example/Arduino-v3.3.3/08_LVGL_V8_Test/bsp_lvgl_port.cpp \
  -o firmware-c6/hw/co5300_vendor/bsp_lvgl_port.cpp
curl -fsSL https://raw.githubusercontent.com/waveshareteam/ESP32-C6-Touch-AMOLED-2.16/main/02_Example/Arduino-v3.3.3/08_LVGL_V8_Test/bsp_lvgl_port.h \
  -o firmware-c6/hw/co5300_vendor/bsp_lvgl_port.h
```

Add an attribution comment at the top of each file:

```c
// Vendored from Waveshare's official example for the ESP32-C6-Touch-AMOLED-2.16
// board: https://github.com/waveshareteam/ESP32-C6-Touch-AMOLED-2.16
// Used here for the CO5300 init sequence and QSPI/esp_lcd setup — both are
// not documented elsewhere. Apache-2.0 / per upstream LICENSE.
```

- [ ] **Step 4.2: Create `firmware-c6/hw/co5300.h`**

```c
#pragma once
#include <Arduino.h>
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

// Initialize the CO5300 over QSPI.
// Must be called AFTER pmic_init().
// Returns true on success; populates panel handles in the OUT params.
bool co5300_init(esp_lcd_panel_io_handle_t *io_out,
                 esp_lcd_panel_handle_t *panel_out);

// Set AMOLED brightness 0..255 via DCS 0x51.
void co5300_set_brightness(esp_lcd_panel_io_handle_t io, uint8_t level);
```

- [ ] **Step 4.3: Create `firmware-c6/hw/co5300.cpp`**

```c
#include "co5300.h"
#include "pins.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "driver/spi_master.h"

// Forward-declare into the vendored file. The vendored TU defines an
// init function that does the SPI bus + panel_io + custom-init-sequence
// setup. We expose a thin wrapper here so callers don't include vendored
// headers directly.
extern "C" {
bool waveshare_co5300_bring_up(esp_lcd_panel_io_handle_t *io_out,
                               esp_lcd_panel_handle_t *panel_out);
}

bool co5300_init(esp_lcd_panel_io_handle_t *io_out,
                 esp_lcd_panel_handle_t *panel_out) {
  if (!waveshare_co5300_bring_up(io_out, panel_out)) {
    Serial.println("CO5300: bring_up failed");
    return false;
  }
  Serial.println("CO5300: init OK");
  return true;
}

void co5300_set_brightness(esp_lcd_panel_io_handle_t io, uint8_t level) {
  // DCS 0x51 = Write Display Brightness.
  uint8_t param = level;
  esp_lcd_panel_io_tx_param(io, 0x51, &param, 1);
}
```

**Wrap the vendored upstream function:** in `firmware-c6/hw/co5300_vendor/bsp_lvgl_port.cpp`, find the existing initialization function (it will be named something like `bsp_lvgl_init` or `Lcd_Init`). At the bottom of that file, add an extern "C" wrapper that calls the existing init internals but stops before the LVGL part — we only want the LCD bring-up here. **Read the vendored file carefully** and identify the section between "SPI bus config" and "before lvgl_init" — extract that into a function named `waveshare_co5300_bring_up`. If the vendored code does it all in one function, copy the upper portion (everything before `lv_init()`) into the new function.

If this proves fiddly (the upstream code is tangled), the simpler alternative is to **leave the vendored file structure as-is, call its main init from co5300.cpp, and accept that LVGL gets initialized one task earlier than planned.** Note this deviation in the commit message; subsequent tasks adapt.

- [ ] **Step 4.4: Solid-color test in `firmware-c6.ino`**

```c
#include <Arduino.h>
#include "hw/i2c_bus.h"
#include "hw/pmic_axp2101.h"
#include "hw/co5300.h"
#include "hw/pins.h"
#include "esp_heap_caps.h"

esp_lcd_panel_io_handle_t g_io   = nullptr;
esp_lcd_panel_handle_t    g_panel = nullptr;

static uint16_t *line_buf = nullptr;   // 480 px × 2 bytes

static void fill_color(uint16_t color565) {
  // Send full screen one row at a time to avoid a 480-row buffer.
  for (int i = 0; i < LCD_H_RES; i++) line_buf[i] = color565;
  for (int y = 0; y < LCD_V_RES; y++) {
    esp_lcd_panel_draw_bitmap(g_panel, 0, y, LCD_H_RES, y + 1, line_buf);
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n=== Claudy-C6 boot ===");

  i2c_bus_begin();
  if (!pmic_init()) { while (true) delay(1000); }
  delay(100);

  if (!co5300_init(&g_io, &g_panel)) { while (true) delay(1000); }
  esp_lcd_panel_disp_on_off(g_panel, true);
  co5300_set_brightness(g_io, 200);

  line_buf = (uint16_t*) heap_caps_malloc(LCD_H_RES * 2, MALLOC_CAP_DMA);
  if (!line_buf) { Serial.println("line_buf alloc fail"); while (true) delay(1000); }

  Serial.printf("Free heap after init: %u bytes\n", ESP.getFreeHeap());
}

void loop() {
  Serial.println("RED");   fill_color(0xF800); delay(1500);
  Serial.println("GREEN"); fill_color(0x07E0); delay(1500);
  Serial.println("BLUE");  fill_color(0x001F); delay(1500);
  Serial.println("WHITE"); fill_color(0xFFFF); delay(1500);
}
```

- [ ] **Step 4.5: Build, flash, verify (Gate G3)**

```bash
./scripts/build-c6.sh && ./scripts/flash-c6.sh && ./scripts/monitor-c6.sh
```

**Gate G3 passes if all four colors render correctly:**
- Red, green, blue, white each fully cover the 480×480 screen
- No pixel corruption, vertical/horizontal stripes, or shifted rows
- Colors look saturated (an AMOLED red is genuinely red; if it looks blue-tinted, byte order may be wrong)

Common problems & fixes:
- **Wrong colors** (red shows as blue): byte-swap. Either swap bytes in the panel config or use `__builtin_bswap16(color)`. Look for an `rgb_order` or `swap_bytes` flag in the vendored init code.
- **Shifted / scrolling image**: wrong `offset_x` / `offset_y` or wrong column/row range in `esp_lcd_panel_set_gap`.
- **Garbled stripes**: QSPI clock too high. Drop from 80MHz to 40MHz.
- **Black screen**: revisit `esp_lcd_panel_disp_on_off(panel, true)` and `co5300_set_brightness(io, 200)` and the DCS 0x29 (display on) command in the init sequence.

Also note the free heap printed — it should be ≥ 320KB at this point. Record the number; subsequent tasks consume heap.

- [ ] **Step 4.6: Commit**

```bash
git add firmware-c6/hw/co5300* firmware-c6/firmware-c6.ino
git commit -m "firmware-c6: CO5300 init via vendored Waveshare BSP; G3 solid-color test passes"
```

---

## Task 5: LVGL Port — Hello World (Gate G4)

**Files:**
- Create: `firmware-c6/ui/lvgl_port.h`
- Create: `firmware-c6/ui/lvgl_port.cpp`
- Create: `firmware-c6/ui/theme.h`
- Create: `firmware-c6/lv_conf.h` (LVGL config — required at sketch root)
- Modify: `firmware-c6/firmware-c6.ino`

**Goal:** LVGL initialized with two partial draw buffers (each 480×48 RGB565 = 46KB). A label "Hello, Claudy" renders centered.

- [ ] **Step 5.1: Create `firmware-c6/lv_conf.h`**

LVGL requires this file at the sketch root. Copy `lv_conf_template.h` from the LVGL library install path:

```bash
cp ~/Library/Arduino15/libraries/lvgl/lv_conf_template.h firmware-c6/lv_conf.h
```

Then edit `firmware-c6/lv_conf.h`:

1. Find `#if 0  /*Set it to "1" to enable content*/` near the top → change `0` to `1`.
2. Find `#define LV_COLOR_DEPTH` → set to `16`.
3. Find `#define LV_COLOR_16_SWAP` → set to `0` (leave default; swap if Gate 4 colors look wrong).
4. Find `#define LV_FONT_MONTSERRAT_16` → set to `1`.
5. Find `#define LV_FONT_MONTSERRAT_24` → set to `1`.
6. Find `#define LV_FONT_MONTSERRAT_28` → set to `1`.
7. Find `#define LV_FONT_SIMSUN_16_CJK` → set to `1` (CJK message line).
8. Find `#define LV_USE_CANVAS` → set to `1`.
9. Find `#define LV_USE_PERF_MONITOR` → set to `0` (optional; enable later for debug).

- [ ] **Step 5.2: Create `firmware-c6/ui/theme.h`**

```c
#pragma once
#include <stdint.h>

// Layout (480x480 screen, layout B from spec: upper half mascot, lower half info)
constexpr int LCD_W = 480;
constexpr int LCD_H = 480;

// Upper half — mascot canvas region
constexpr int MASCOT_X = 0;
constexpr int MASCOT_Y = 0;
constexpr int MASCOT_W = 480;
constexpr int MASCOT_H = 240;     // 480x240 = 230KB; may shrink at G5 (see spec §7)

// Lower half — info region
constexpr int INFO_X = 24;
constexpr int INFO_Y = 256;
constexpr int INFO_W = 432;       // padding 24 each side
constexpr int INFO_H = 200;       // 256..456
constexpr int BAR_Y  = 420;
constexpr int BAR_H  = 18;

// RGB565 palette (same constants as firmware/mascot.cpp + display.cpp)
constexpr uint16_t COL_BG       = 0x0000;   // black
constexpr uint16_t COL_FG       = 0xFFFF;   // white
constexpr uint16_t COL_DIM      = 0xBDF7;   // light gray
constexpr uint16_t COL_BAR_OK   = 0x06DF;   // teal
constexpr uint16_t COL_BAR_WARN = 0xFD20;   // orange
constexpr uint16_t COL_BAR_HOT  = 0xF800;   // red

// Mascot palette
constexpr uint16_t COL_CLAUDE_ORANGE = 0xCC2D;
constexpr uint16_t COL_CLAUDE_DARK   = 0x1082;
constexpr uint16_t COL_CLAUDE_DIM    = 0x4208;
constexpr uint16_t COL_ACCENT_THINK  = 0x7E5F;
constexpr uint16_t COL_ACCENT_WORK   = 0x05FF;
constexpr uint16_t COL_ACCENT_WAIT   = 0xFEC0;
constexpr uint16_t COL_ACCENT_ERROR  = 0xF820;
constexpr uint16_t COL_ACCENT_DONE   = 0x07E5;
```

- [ ] **Step 5.3: Create `firmware-c6/ui/lvgl_port.h`**

```c
#pragma once
#include <Arduino.h>
#include "lvgl.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

// Initialize LVGL and bind it to the already-initialized CO5300 panel.
// Allocates two 92KB partial draw buffers in DRAM.
bool lvgl_port_init(esp_lcd_panel_io_handle_t io, esp_lcd_panel_handle_t panel);

// Drive from loop(). Calls lv_timer_handler. Returns the ms to wait until
// the next call (use as a delay).
uint32_t lvgl_port_tick();

// Mutex around LVGL state — required because the HTTP handler (web server
// task on core 0) and the main loop both touch widgets.
bool lvgl_port_lock(int timeout_ms);
void lvgl_port_unlock();
```

- [ ] **Step 5.4: Create `firmware-c6/ui/lvgl_port.cpp`**

```c
#include "lvgl_port.h"
#include "../hw/pins.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static esp_lcd_panel_io_handle_t s_io;
static esp_lcd_panel_handle_t    s_panel;
static lv_disp_draw_buf_t        s_draw_buf;
static lv_color_t               *s_buf1 = nullptr;
static lv_color_t               *s_buf2 = nullptr;
static SemaphoreHandle_t         s_mutex = nullptr;

static constexpr size_t LVGL_BUF_LINES = 48;
static constexpr size_t LVGL_BUF_PX    = LCD_H_RES * LVGL_BUF_LINES;

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map) {
  esp_lcd_panel_draw_bitmap(s_panel, area->x1, area->y1,
                            area->x2 + 1, area->y2 + 1, color_map);
  lv_disp_flush_ready(drv);
}

bool lvgl_port_init(esp_lcd_panel_io_handle_t io, esp_lcd_panel_handle_t panel) {
  s_io = io;
  s_panel = panel;
  s_mutex = xSemaphoreCreateRecursiveMutex();
  if (!s_mutex) { Serial.println("lvgl_port: mutex alloc fail"); return false; }

  lv_init();

  s_buf1 = (lv_color_t*) heap_caps_aligned_alloc(4, LVGL_BUF_PX * sizeof(lv_color_t),
                                                 MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  s_buf2 = (lv_color_t*) heap_caps_aligned_alloc(4, LVGL_BUF_PX * sizeof(lv_color_t),
                                                 MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  if (!s_buf1 || !s_buf2) {
    Serial.printf("lvgl_port: draw buf alloc fail (need 2x %u bytes)\n",
                  (unsigned)(LVGL_BUF_PX * sizeof(lv_color_t)));
    return false;
  }
  lv_disp_draw_buf_init(&s_draw_buf, s_buf1, s_buf2, LVGL_BUF_PX);

  static lv_disp_drv_t drv;
  lv_disp_drv_init(&drv);
  drv.hor_res = LCD_H_RES;
  drv.ver_res = LCD_V_RES;
  drv.flush_cb = flush_cb;
  drv.draw_buf = &s_draw_buf;
  lv_disp_drv_register(&drv);

  Serial.printf("lvgl_port: ready (2x%u byte draw buffers in DMA-capable DRAM)\n",
                (unsigned)(LVGL_BUF_PX * sizeof(lv_color_t)));
  return true;
}

uint32_t lvgl_port_tick() {
  if (!lvgl_port_lock(0)) return 5;
  uint32_t next = lv_timer_handler();
  lvgl_port_unlock();
  if (next > 500) next = 500;
  if (next < 5)   next = 5;
  return next;
}

bool lvgl_port_lock(int timeout_ms) {
  if (!s_mutex) return false;
  return xSemaphoreTakeRecursive(s_mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void lvgl_port_unlock() {
  if (s_mutex) xSemaphoreGiveRecursive(s_mutex);
}
```

> **LVGL tick source:** the Arduino-ESP32 LVGL library has built-in millis()-based tick support if `LV_TICK_CUSTOM 1` and `LV_TICK_CUSTOM_INCLUDE "Arduino.h"` are set in `lv_conf.h`. Confirm those defaults are on. If not, set them in Step 5.1.

- [ ] **Step 5.5: Hello-world in `firmware-c6.ino`**

```c
#include <Arduino.h>
#include "lvgl.h"
#include "hw/i2c_bus.h"
#include "hw/pmic_axp2101.h"
#include "hw/co5300.h"
#include "hw/pins.h"
#include "ui/lvgl_port.h"
#include "ui/theme.h"

esp_lcd_panel_io_handle_t g_io;
esp_lcd_panel_handle_t    g_panel;

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n=== Claudy-C6 boot ===");

  i2c_bus_begin();
  if (!pmic_init()) while (true) delay(1000);
  delay(100);

  if (!co5300_init(&g_io, &g_panel)) while (true) delay(1000);
  esp_lcd_panel_disp_on_off(g_panel, true);
  co5300_set_brightness(g_io, 200);

  if (!lvgl_port_init(g_io, g_panel)) while (true) delay(1000);

  // Build a hello screen
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_make(0, 0, 0), 0);

  lv_obj_t *label = lv_label_create(scr);
  lv_label_set_text(label, "Hello, Claudy");
  lv_obj_set_style_text_color(label, lv_color_make(0xFF, 0xFF, 0xFF), 0);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_28, 0);
  lv_obj_center(label);

  Serial.printf("Free heap after UI init: %u bytes\n", ESP.getFreeHeap());
}

void loop() {
  uint32_t delay_ms = lvgl_port_tick();
  delay(delay_ms);
}
```

- [ ] **Step 5.6: Build, flash, verify (Gate G4)**

```bash
./scripts/build-c6.sh && ./scripts/flash-c6.sh && ./scripts/monitor-c6.sh
```

**Gate G4 passes if "Hello, Claudy" appears centered on the screen in white on black.**

Check serial output:
- `lvgl_port: ready (2x92160 byte draw buffers in DMA-capable DRAM)`
- `Free heap after UI init: <number>` — record this. Should be ≥ 250KB. If lower, Task 6's mascot canvas will need to shrink.

Common problems & fixes:
- **Garbled text / wrong colors**: toggle `LV_COLOR_16_SWAP` in `lv_conf.h` between 0 and 1, rebuild.
- **Nothing renders**: confirm `flush_cb` is being called (add a `Serial.println` inside it temporarily). If not, LVGL didn't register the display — re-check `lv_disp_drv_register`.

- [ ] **Step 5.7: Commit**

```bash
git add firmware-c6/lv_conf.h firmware-c6/ui/ firmware-c6/firmware-c6.ino
git commit -m "firmware-c6: LVGL hello world; G4 passes (partial buf 2x46KB)"
```

---

## Task 6: Mascot Canvas + Procedural Drawing (Gate G5)

**Files:**
- Create: `firmware-c6/ui/gfx.h`
- Create: `firmware-c6/ui/gfx.cpp`
- Create: `firmware-c6/ui/mascot.h`
- Create: `firmware-c6/ui/mascot.cpp`
- Modify: `firmware-c6/firmware-c6.ino`

**Goal:** A 480×240 (or smaller, see fallback) `lv_canvas_t` in the upper half. The pixel-grid mascot is drawn into it. An `lv_timer` ticks at 33ms and redraws so animations work. Idle state is visible.

- [ ] **Step 6.1: Decide mascot canvas size based on Task 5 free-heap reading**

| Free heap after UI init | Mascot canvas size | Buffer size |
|---|---|---|
| ≥ 290 KB | 480×240 | 230 KB |
| 230..290 KB | 400×240 | 188 KB |
| < 230 KB | 360×240 | 172 KB |

In `firmware-c6/ui/theme.h`, set `MASCOT_W` and `MASCOT_H` to match. The mascot is centered within the upper half automatically by the layout (we set `MASCOT_X` to `(LCD_W - MASCOT_W) / 2`).

If you picked smaller, also update `MASCOT_X`:

```c
constexpr int MASCOT_W = 400;
constexpr int MASCOT_X = (480 - 400) / 2;   // = 40
```

- [ ] **Step 6.2: Create `firmware-c6/ui/gfx.h`**

```c
#pragma once
#include "lvgl.h"

// Thin facade over lv_canvas drawing for mascot.cpp.
// All coordinates are local to the canvas.
void gfx_fill_bg(lv_obj_t *canvas, lv_color_t c);
void gfx_fill_rect(lv_obj_t *canvas, int x, int y, int w, int h, lv_color_t c);
void gfx_fill_circle(lv_obj_t *canvas, int cx, int cy, int r, lv_color_t c);

// Convert an RGB565 uint16_t to lv_color_t (handles LV_COLOR_16_SWAP if enabled).
static inline lv_color_t rgb565_to_lv(uint16_t v) {
  return lv_color_make((v >> 11 & 0x1F) << 3,
                       (v >> 5  & 0x3F) << 2,
                       (v       & 0x1F) << 3);
}
```

- [ ] **Step 6.3: Create `firmware-c6/ui/gfx.cpp`**

```c
#include "gfx.h"

void gfx_fill_bg(lv_obj_t *canvas, lv_color_t c) {
  lv_canvas_fill_bg(canvas, c, LV_OPA_COVER);
}

void gfx_fill_rect(lv_obj_t *canvas, int x, int y, int w, int h, lv_color_t c) {
  lv_draw_rect_dsc_t dsc;
  lv_draw_rect_dsc_init(&dsc);
  dsc.bg_color = c;
  dsc.bg_opa   = LV_OPA_COVER;
  dsc.border_width = 0;
  dsc.radius   = 0;
  lv_canvas_draw_rect(canvas, x, y, w, h, &dsc);
}

void gfx_fill_circle(lv_obj_t *canvas, int cx, int cy, int r, lv_color_t c) {
  // Implement as a filled rect with radius = LV_RADIUS_CIRCLE on an equal-side bbox.
  lv_draw_rect_dsc_t dsc;
  lv_draw_rect_dsc_init(&dsc);
  dsc.bg_color = c;
  dsc.bg_opa   = LV_OPA_COVER;
  dsc.border_width = 0;
  dsc.radius   = LV_RADIUS_CIRCLE;
  lv_canvas_draw_rect(canvas, cx - r, cy - r, r * 2, r * 2, &dsc);
}
```

- [ ] **Step 6.4: Create `firmware-c6/ui/mascot.h`**

```c
#pragma once
#include "lvgl.h"
#include "../state.h"

// Create the mascot canvas as a child of `parent`, positioned per theme.h.
// Allocates the canvas pixel buffer (MASCOT_W * MASCOT_H * 2 bytes).
// Returns the canvas object.
lv_obj_t* mascot_create(lv_obj_t *parent);

// Redraw the mascot for the given state. Call from your animation timer.
void mascot_draw(lv_obj_t *canvas, MascotState state);

// Animation interval (ms) for the given state. 0 = static (no redraw needed).
uint32_t mascot_anim_interval(MascotState state);
```

- [ ] **Step 6.5: Create `firmware-c6/ui/mascot.cpp`**

This is a direct port of `firmware/mascot.cpp` with the LovyanGFX API calls swapped for the `gfx_*` facade. The pixel-grid art and animation phase logic are identical.

```c
#include "mascot.h"
#include "gfx.h"
#include "theme.h"
#include "esp_heap_caps.h"
#include <Arduino.h>
#include <math.h>

namespace {
// 11 cols × 9 rows pixel grid. Same art as firmware/mascot.cpp.
constexpr int ROWS = 9;
constexpr int COLS = 11;

const char* CLAUDY_OPEN[] = {
  "           ",
  " ######### ",
  " ######### ",
  " ##.###.## ",
  "###.###.###",
  "###########",
  " ######### ",
  " # #   # # ",
  " # #   # # ",
};
const char* CLAUDY_BLINK[] = {
  "           ",
  " ######### ",
  " ######### ",
  " ######### ",
  "###########",
  "###########",
  " ######### ",
  " # #   # # ",
  " # #   # # ",
};
const char* CLAUDY_SQUINT[] = {
  "           ",
  " ######### ",
  " ##.###.## ",
  " ##.###.## ",
  "###########",
  "###########",
  " ######### ",
  " # #   # # ",
  " # #   # # ",
};
const char* CLAUDY_X_EYES[] = {
  "           ",
  " ######### ",
  " ##.###.## ",
  " ##.###.## ",
  "###########",
  "###########",
  " ######### ",
  " # #   # # ",
  " # #   # # ",
};

void draw_claudy(lv_obj_t *canvas, int cx, int cy, int px,
                 const char* const* grid,
                 lv_color_t body, lv_color_t eye, int offX, int offY) {
  const int x0 = cx - (COLS * px) / 2 + offX;
  const int y0 = cy - (ROWS * px) / 2 + offY;
  for (int r = 0; r < ROWS; r++) {
    const char* row = grid[r];
    for (int c = 0; c < COLS; c++) {
      char ch = row[c];
      if (ch == '#')      gfx_fill_rect(canvas, x0 + c * px, y0 + r * px, px, px, body);
      else if (ch == '.') gfx_fill_rect(canvas, x0 + c * px, y0 + r * px, px, px, eye);
    }
  }
}
}  // namespace

lv_obj_t* mascot_create(lv_obj_t *parent) {
  const size_t bytes = MASCOT_W * MASCOT_H * 2;
  void *buf = heap_caps_malloc(bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  if (!buf) {
    Serial.printf("mascot_create: alloc fail (%u bytes)\n", (unsigned)bytes);
    return nullptr;
  }
  lv_obj_t *canvas = lv_canvas_create(parent);
  lv_canvas_set_buffer(canvas, buf, MASCOT_W, MASCOT_H, LV_IMG_CF_TRUE_COLOR);
  lv_obj_set_pos(canvas, MASCOT_X, MASCOT_Y);
  gfx_fill_bg(canvas, lv_color_make(0, 0, 0));
  Serial.printf("mascot_create: canvas %dx%d (%u bytes)\n",
                MASCOT_W, MASCOT_H, (unsigned)bytes);
  return canvas;
}

uint32_t mascot_anim_interval(MascotState s) {
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

void mascot_draw(lv_obj_t *canvas, MascotState state) {
  if (!canvas) return;
  gfx_fill_bg(canvas, lv_color_make(0, 0, 0));

  const int cx = MASCOT_W / 2;
  const int cy = MASCOT_H / 2;

  // Largest pixel size that fits with margin.
  int px = 16;
  if (px * COLS > MASCOT_W - 20) px = (MASCOT_W - 20) / COLS;
  if (px * ROWS > MASCOT_H - 20) px = (MASCOT_H - 20) / ROWS;

  unsigned long t = millis();
  int dx = 0, dy = 0;
  lv_color_t body = rgb565_to_lv(COL_CLAUDE_ORANGE);
  lv_color_t eye  = rgb565_to_lv(COL_CLAUDE_DARK);
  const char* const* grid = CLAUDY_OPEN;

  switch (state) {
    case STATE_IDLE:
      dy = (int)(sinf((float)t / 1500.0f * 2.0f * (float)PI) * 2.0f);
      if ((t % 3500) < 150) grid = CLAUDY_BLINK;
      break;
    case STATE_THINKING:
      dy = (int)(sinf((float)t / 700.0f * 2.0f * (float)PI) * 1.5f);
      grid = CLAUDY_SQUINT;
      body = rgb565_to_lv(COL_ACCENT_THINK);
      break;
    case STATE_WORKING:
      dy = (int)(sinf((float)t / 250.0f * 2.0f * (float)PI) * 1.5f);
      dx = (int)(sinf((float)t / 500.0f * 2.0f * (float)PI) * 1.0f);
      body = rgb565_to_lv(COL_ACCENT_WORK);
      break;
    case STATE_WAITING:
      dy = (int)(fabsf(sinf((float)t / 900.0f * 2.0f * (float)PI)) * -3.0f);
      body = rgb565_to_lv(COL_ACCENT_WAIT);
      break;
    case STATE_ERROR:
      dx = (int)(sinf((float)t / 50.0f * 2.0f * (float)PI) * 4.0f);
      grid = CLAUDY_X_EYES;
      body = rgb565_to_lv(COL_ACCENT_ERROR);
      break;
    case STATE_DONE:
      dy = (int)(-fabsf(sinf((float)t / 350.0f * 2.0f * (float)PI)) * 5.0f);
      body = rgb565_to_lv(COL_ACCENT_DONE);
      if ((t % 700) < 100) grid = CLAUDY_BLINK;
      break;
    case STATE_BOOT:
      body = rgb565_to_lv(COL_CLAUDE_DIM);
      break;
  }

  draw_claudy(canvas, cx, cy, px, grid, body, eye, dx, dy);
  lv_obj_invalidate(canvas);
}
```

- [ ] **Step 6.6: Add mascot to `firmware-c6.ino`**

Replace `setup()`'s "Build a hello screen" block with:

```c
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_make(0, 0, 0), 0);

  lv_obj_t *mascot = mascot_create(scr);
  if (!mascot) {
    Serial.println("FATAL: mascot canvas alloc failed; shrink MASCOT_W/H in theme.h");
    while (true) delay(1000);
  }

  static MascotState anim_state = STATE_IDLE;
  static lv_timer_t *mascot_timer = lv_timer_create(
    [](lv_timer_t *t) {
      lv_obj_t *c = (lv_obj_t*) t->user_data;
      mascot_draw(c, anim_state);
    }, mascot_anim_interval(STATE_IDLE), mascot);
  (void)mascot_timer;

  Serial.printf("Free heap after mascot: %u bytes\n", ESP.getFreeHeap());
```

Add `#include "ui/mascot.h"` at the top.

- [ ] **Step 6.7: Build, flash, verify (Gate G5)**

```bash
./scripts/build-c6.sh && ./scripts/flash-c6.sh && ./scripts/monitor-c6.sh
```

**Gate G5 passes if:**
- An orange pixel-mascot face appears in the upper half of the screen
- It bobs gently up and down (idle breathing animation)
- It blinks every ~3.5 seconds
- Free heap remains ≥ 30KB after mascot creation

If free heap < 30KB → shrink mascot canvas per Step 6.1 table, rebuild, retest.

If mascot pixels look wrong colors → `LV_COLOR_16_SWAP` mismatch. Toggle and rebuild.

- [ ] **Step 6.8: Commit**

```bash
git add firmware-c6/ui/gfx.{h,cpp} firmware-c6/ui/mascot.{h,cpp} firmware-c6/ui/theme.h firmware-c6/firmware-c6.ino
git commit -m "firmware-c6: mascot canvas with idle animation; G5 passes"
```

---

## Task 7: CST9220 Touch Driver — Raw Coordinates (Gate G6)

**Files:**
- Create: `firmware-c6/hw/touch_cst9220.h`
- Create: `firmware-c6/hw/touch_cst9220.cpp`
- Modify: `firmware-c6/firmware-c6.ino`

**Goal:** Touching the screen prints raw X/Y coordinates over serial. The INT pin triggers correctly.

- [ ] **Step 7.1: Create `firmware-c6/hw/touch_cst9220.h`**

```c
#pragma once
#include <Arduino.h>

bool touch_init();

// Non-blocking read. Returns true if a fresh touch is available;
// outputs coordinates in display pixel space (0..LCD_W, 0..LCD_H).
bool touch_read(uint16_t *x, uint16_t *y);
```

- [ ] **Step 7.2: Create `firmware-c6/hw/touch_cst9220.cpp`**

The CST9220 read protocol is shown in Waveshare's `lcd_touch.cpp` (already inspected during spec-writing). The data sheet register at `0xD000` returns a 10-byte structure: `[status, x_hi, y_hi, lo_pack, ?, points, magic=0xAB, ...]`.

```c
#include "touch_cst9220.h"
#include "i2c_bus.h"
#include "pins.h"
#include <Wire.h>

namespace {
volatile bool s_int_flag = false;

void IRAM_ATTR touch_isr() {
  s_int_flag = true;
}
}  // namespace

bool touch_init() {
  // Reset pulse
  pinMode(PIN_TOUCH_RST, OUTPUT);
  digitalWrite(PIN_TOUCH_RST, 1); delay(50);
  digitalWrite(PIN_TOUCH_RST, 0); delay(50);
  digitalWrite(PIN_TOUCH_RST, 1); delay(200);

  if (!i2c_probe(TOUCH_I2C_ADDR)) {
    Serial.println("touch: CST9220 not found at 0x5A");
    return false;
  }

  pinMode(PIN_TOUCH_INT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_TOUCH_INT), touch_isr, FALLING);

  Serial.println("touch: CST9220 ready");
  return true;
}

bool touch_read(uint16_t *x, uint16_t *y) {
  if (!s_int_flag) return false;
  s_int_flag = false;

  // Write 2-byte register pointer 0xD000, then read 10 bytes.
  uint8_t out[2] = { 0xD0, 0x00 };
  uint8_t data[10] = { 0 };

  Wire.beginTransmission(TOUCH_I2C_ADDR);
  Wire.write(out, 2);
  if (Wire.endTransmission(false) != 0) return false;

  Wire.requestFrom((int)TOUCH_I2C_ADDR, 10);
  size_t i = 0;
  while (Wire.available() && i < 10) data[i++] = Wire.read();
  if (i < 10) return false;

  if (data[6] != 0xAB) return false;          // magic byte check
  uint8_t points = data[5] & 0x7F;
  if (points == 0) return false;
  uint8_t status = data[0] & 0x0F;
  if (status != 0x06) return false;           // not "press" event

  uint16_t ry = ((uint16_t)data[1] << 4) | (data[3] >> 4);
  uint16_t rx = ((uint16_t)data[2] << 4) | (data[3] & 0x0F);
  // X axis is mirrored vs display per Waveshare reference.
  *x = LCD_H_RES - rx;
  *y = ry;
  return true;
}
```

- [ ] **Step 7.3: Wire into `firmware-c6.ino`**

Add `#include "hw/touch_cst9220.h"`. In `setup()` after `lvgl_port_init`, add:

```c
  if (!touch_init()) {
    Serial.println("WARN: touch init failed (continuing without touch)");
  }
```

In `loop()`, add before `lvgl_port_tick()`:

```c
  uint16_t tx, ty;
  if (touch_read(&tx, &ty)) {
    Serial.printf("touch: x=%u y=%u\n", tx, ty);
  }
```

- [ ] **Step 7.4: Build, flash, verify (Gate G6)**

```bash
./scripts/build-c6.sh && ./scripts/flash-c6.sh && ./scripts/monitor-c6.sh
```

Touch the screen at four corners and the middle. **Gate G6 passes if:**
- Top-left tap: `x` ≈ 0, `y` ≈ 0
- Top-right: `x` ≈ 480, `y` ≈ 0
- Bottom-right: `x` ≈ 480, `y` ≈ 480
- Bottom-left: `x` ≈ 0, `y` ≈ 480
- Middle: both ≈ 240

If axes are swapped or inverted: try removing the `LCD_H_RES - rx` mirror, or swap `*x` and `*y`. If nothing fires at all: check that I2C scan still shows 0x5A; check `PIN_TOUCH_INT` is hooked up (try changing `FALLING` to `CHANGE`).

- [ ] **Step 7.5: Commit**

```bash
git add firmware-c6/hw/touch_cst9220.{h,cpp} firmware-c6/firmware-c6.ino
git commit -m "firmware-c6: CST9220 touch driver; G6 passes (raw XY coords reported)"
```

---

## Task 8: Full UI Widgets (state label, tool chip, message, token bar)

**Files:**
- Create: `firmware-c6/ui/ui.h`
- Create: `firmware-c6/ui/ui.cpp`
- Modify: `firmware-c6/firmware-c6.ino`

**Goal:** The lower half of the screen shows: state name, tool chip + tool name, message (up to 2 lines, CJK supported), token bar + "85k / 200k" text + "42%" percentage. All update when `g_state` changes.

- [ ] **Step 8.1: Create `firmware-c6/ui/ui.h`**

```c
#pragma once
#include "lvgl.h"
#include "../state.h"

extern AppState g_state;     // defined in firmware-c6.ino

void ui_init();              // builds widgets; call after lvgl_port_init
void ui_apply_state();       // call after mutating g_state to refresh widgets
```

- [ ] **Step 8.2: Create `firmware-c6/ui/ui.cpp`**

```c
#include "ui.h"
#include "mascot.h"
#include "theme.h"
#include "lvgl_port.h"
#include "gfx.h"
#include <Arduino.h>

namespace {
lv_obj_t *s_mascot;
lv_obj_t *s_state_label;
lv_obj_t *s_chip_bg;
lv_obj_t *s_chip_label;
lv_obj_t *s_tool_label;
lv_obj_t *s_msg_label;
lv_obj_t *s_bar;
lv_obj_t *s_bar_pct_label;
lv_obj_t *s_bar_detail_label;

MascotState s_last_anim_state = STATE_BOOT;
lv_timer_t *s_anim_timer = nullptr;

void anim_cb(lv_timer_t *t) {
  mascot_draw(s_mascot, g_state.state);
  // Adjust period if state changed
  uint32_t want = mascot_anim_interval(g_state.state);
  if (want > 0 && want != lv_timer_get_period(t)) lv_timer_set_period(t, want);
}

const char* tool_glyph(ToolIcon t) {
  switch (t) {
    case TOOL_READ:  return "R";
    case TOOL_EDIT:  return "E";
    case TOOL_WRITE: return "W";
    case TOOL_BASH:  return "$";
    case TOOL_GREP:  return "G";
    case TOOL_WEB:   return "@";
    case TOOL_TASK:  return "T";
    default:         return "*";
  }
}

lv_color_t tool_color(ToolIcon t) {
  switch (t) {
    case TOOL_READ:  return rgb565_to_lv(0x3475);
    case TOOL_EDIT:  return rgb565_to_lv(0xC9A0);
    case TOOL_WRITE: return rgb565_to_lv(0xCAA0);
    case TOOL_BASH:  return rgb565_to_lv(0x39E7);
    case TOOL_GREP:  return rgb565_to_lv(0x4A89);
    case TOOL_WEB:   return rgb565_to_lv(0x2C9F);
    case TOOL_TASK:  return rgb565_to_lv(0x880F);
    default:         return rgb565_to_lv(0x528A);
  }
}
}  // namespace

void ui_init() {
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

  s_mascot = mascot_create(scr);
  if (!s_mascot) {
    Serial.println("FATAL: mascot alloc failed");
    while (true) delay(1000);
  }
  s_anim_timer = lv_timer_create(anim_cb, mascot_anim_interval(STATE_IDLE), nullptr);

  // State label (top of info region)
  s_state_label = lv_label_create(scr);
  lv_label_set_text(s_state_label, "Boot");
  lv_obj_set_style_text_font(s_state_label, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(s_state_label, lv_color_white(), 0);
  lv_obj_set_pos(s_state_label, INFO_X, INFO_Y);

  // Tool chip
  s_chip_bg = lv_obj_create(scr);
  lv_obj_set_size(s_chip_bg, 36, 28);
  lv_obj_set_pos(s_chip_bg, INFO_X, INFO_Y + 50);
  lv_obj_set_style_radius(s_chip_bg, 6, 0);
  lv_obj_set_style_border_width(s_chip_bg, 0, 0);
  lv_obj_set_style_pad_all(s_chip_bg, 0, 0);
  lv_obj_clear_flag(s_chip_bg, LV_OBJ_FLAG_SCROLLABLE);

  s_chip_label = lv_label_create(s_chip_bg);
  lv_obj_set_style_text_font(s_chip_label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(s_chip_label, lv_color_white(), 0);
  lv_obj_center(s_chip_label);
  lv_label_set_text(s_chip_label, "");

  // Tool name (right of chip)
  s_tool_label = lv_label_create(scr);
  lv_obj_set_style_text_font(s_tool_label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(s_tool_label, rgb565_to_lv(COL_DIM), 0);
  lv_obj_set_pos(s_tool_label, INFO_X + 46, INFO_Y + 56);
  lv_label_set_text(s_tool_label, "");

  // Message (2 lines, CJK)
  s_msg_label = lv_label_create(scr);
  lv_obj_set_width(s_msg_label, INFO_W);
  lv_label_set_long_mode(s_msg_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(s_msg_label, &lv_font_simsun_16_cjk, 0);
  lv_obj_set_style_text_color(s_msg_label, rgb565_to_lv(COL_DIM), 0);
  lv_obj_set_pos(s_msg_label, INFO_X, INFO_Y + 96);
  lv_label_set_text(s_msg_label, "Starting...");

  // Token bar
  s_bar = lv_bar_create(scr);
  lv_obj_set_size(s_bar, INFO_W - 100, BAR_H);
  lv_obj_set_pos(s_bar, INFO_X, BAR_Y);
  lv_bar_set_range(s_bar, 0, 1000);   // we'll scale used/max into 0..1000
  lv_bar_set_value(s_bar, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(s_bar, rgb565_to_lv(0x4A49), LV_PART_MAIN);
  lv_obj_set_style_bg_color(s_bar, rgb565_to_lv(COL_BAR_OK), LV_PART_INDICATOR);

  // Big percent (right of bar)
  s_bar_pct_label = lv_label_create(scr);
  lv_obj_set_style_text_font(s_bar_pct_label, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(s_bar_pct_label, lv_color_white(), 0);
  lv_obj_set_pos(s_bar_pct_label, INFO_X + (INFO_W - 100) + 12, BAR_Y - 6);
  lv_label_set_text(s_bar_pct_label, "");

  // Detail (above bar)
  s_bar_detail_label = lv_label_create(scr);
  lv_obj_set_style_text_font(s_bar_detail_label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(s_bar_detail_label, rgb565_to_lv(0x8C71), 0);
  lv_obj_set_pos(s_bar_detail_label, INFO_X, BAR_Y - 24);
  lv_label_set_text(s_bar_detail_label, "");
}

void ui_apply_state() {
  lv_label_set_text(s_state_label, stateName(g_state.state));

  if (g_state.tool == TOOL_NONE) {
    lv_obj_add_flag(s_chip_bg, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_tool_label, "");
  } else {
    lv_obj_clear_flag(s_chip_bg, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(s_chip_bg, tool_color(g_state.tool), 0);
    lv_label_set_text(s_chip_label, tool_glyph(g_state.tool));
    lv_label_set_text(s_tool_label, toolName(g_state.tool));
  }

  lv_label_set_text(s_msg_label, g_state.message);

  if (g_state.tokensMax > 0) {
    int pct1000 = (int)((uint64_t)g_state.tokensUsed * 1000 / g_state.tokensMax);
    if (pct1000 > 1000) pct1000 = 1000;
    lv_bar_set_value(s_bar, pct1000, LV_ANIM_ON);

    // Indicator color crosses thresholds
    lv_color_t c = rgb565_to_lv(COL_BAR_OK);
    if (pct1000 > 750) c = rgb565_to_lv(COL_BAR_WARN);
    if (pct1000 > 900) c = rgb565_to_lv(COL_BAR_HOT);
    lv_obj_set_style_bg_color(s_bar, c, LV_PART_INDICATOR);

    char pctbuf[8];
    snprintf(pctbuf, sizeof(pctbuf), "%d%%", pct1000 / 10);
    lv_label_set_text(s_bar_pct_label, pctbuf);

    char detbuf[32];
    if (g_state.tokensMax >= 1000000) {
      snprintf(detbuf, sizeof(detbuf), "%lu.%luk / 1M",
               (unsigned long)(g_state.tokensUsed / 1000),
               (unsigned long)((g_state.tokensUsed % 1000) / 100));
    } else {
      snprintf(detbuf, sizeof(detbuf), "%luk / %luk",
               (unsigned long)(g_state.tokensUsed / 1000),
               (unsigned long)(g_state.tokensMax / 1000));
    }
    lv_label_set_text(s_bar_detail_label, detbuf);
  } else {
    lv_label_set_text(s_bar_pct_label, "");
    lv_label_set_text(s_bar_detail_label, "");
    lv_bar_set_value(s_bar, 0, LV_ANIM_OFF);
  }
}
```

- [ ] **Step 8.3: Update `firmware-c6.ino` to use `ui_init` + cycle states**

Replace `setup()` mascot-only code with full ui_init. Add a temporary state-cycler in `loop()` so we can visually verify all 6 states without WiFi yet.

```c
#include <Arduino.h>
#include "lvgl.h"
#include "state.h"
#include "hw/i2c_bus.h"
#include "hw/pmic_axp2101.h"
#include "hw/co5300.h"
#include "hw/touch_cst9220.h"
#include "hw/pins.h"
#include "ui/lvgl_port.h"
#include "ui/ui.h"
#include "ui/theme.h"

AppState g_state;
esp_lcd_panel_io_handle_t g_io;
esp_lcd_panel_handle_t    g_panel;

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n=== Claudy-C6 boot ===");

  i2c_bus_begin();
  if (!pmic_init()) while (true) delay(1000);
  delay(100);
  if (!co5300_init(&g_io, &g_panel)) while (true) delay(1000);
  esp_lcd_panel_disp_on_off(g_panel, true);
  co5300_set_brightness(g_io, 200);

  if (!lvgl_port_init(g_io, g_panel)) while (true) delay(1000);
  touch_init();

  if (lvgl_port_lock(200)) {
    ui_init();
    g_state.state = STATE_IDLE;
    g_state.tool = TOOL_NONE;
    strncpy(g_state.message, "Idle", sizeof(g_state.message));
    g_state.tokensUsed = 12345;
    g_state.tokensMax = 200000;
    ui_apply_state();
    lvgl_port_unlock();
  }
}

const MascotState CYCLE[] = {
  STATE_IDLE, STATE_THINKING, STATE_WORKING, STATE_WAITING, STATE_ERROR, STATE_DONE
};
const ToolIcon CYCLE_TOOLS[] = {
  TOOL_NONE, TOOL_NONE, TOOL_EDIT, TOOL_NONE, TOOL_BASH, TOOL_NONE
};
const char *CYCLE_MSGS[] = {
  "Idle",
  "Reading your prompt...",
  "Editing firmware-c6/ui/ui.cpp",
  "Approve write to /etc/hosts?",
  "Bash failed: exit 1",
  "完成 — 3 個檔案已更新",
};

uint8_t cyc = 0;
uint32_t last_change = 0;

void loop() {
  if (millis() - last_change > 3000) {
    last_change = millis();
    if (lvgl_port_lock(100)) {
      g_state.state = CYCLE[cyc];
      g_state.tool  = CYCLE_TOOLS[cyc];
      strncpy(g_state.message, CYCLE_MSGS[cyc], sizeof(g_state.message));
      g_state.tokensUsed += 8000;
      if (g_state.tokensUsed > g_state.tokensMax) g_state.tokensUsed = 12345;
      ui_apply_state();
      lvgl_port_unlock();
    }
    cyc = (cyc + 1) % 6;
  }

  uint16_t tx, ty;
  if (touch_read(&tx, &ty)) Serial.printf("touch: %u,%u\n", tx, ty);

  delay(lvgl_port_tick());
}
```

- [ ] **Step 8.4: Build, flash, verify**

```bash
./scripts/build-c6.sh && ./scripts/flash-c6.sh && ./scripts/monitor-c6.sh
```

**Verify visually:**
- All 6 states cycle every 3s
- Mascot color + animation changes per state
- Tool chip appears for Working / Error, hidden otherwise
- Message updates and CJK characters render in the "完成" line
- Token bar fills proportionally, percent updates, detail text updates
- Bar color transitions teal → orange → red as percent crosses 75 / 90

- [ ] **Step 8.5: Commit**

```bash
git add firmware-c6/ui/ui.{h,cpp} firmware-c6/firmware-c6.ino
git commit -m "firmware-c6: full UI with state/tool/message/token-bar widgets"
```

---

## Task 9: WiFi + mDNS + HTTP API (Gate G7)

**Files:**
- Create: `firmware-c6/net/net.h`
- Create: `firmware-c6/net/net.cpp`
- Modify: `firmware-c6/firmware-c6.ino`

**Goal:** Device connects to WiFi, advertises `claudy.local`, accepts `POST /state` and `GET /state`, mutates `g_state` and calls `ui_apply_state()`. The state cycler from Task 8 is removed.

`firmware/net.cpp` is the source of truth — the only changes are includes (`display.h` → `ui/ui.h`, `requestRedraw()` → `ui_apply_state()`) and adding the `lvgl_port_lock` around UI calls.

- [ ] **Step 9.1: Create `firmware-c6/net/net.h`**

```c
#pragma once
#include <Arduino.h>

bool netBegin();
void netLoop();
bool netIsConnected();
```

- [ ] **Step 9.2: Create `firmware-c6/net/net.cpp`**

```c
#include "net.h"
#include "../state.h"
#include "../ui/ui.h"
#include "../ui/lvgl_port.h"
#include "../config.h"

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>

extern AppState g_state;

static WebServer server(HTTP_PORT);
static bool s_connected = false;

static bool authOk() {
  if (strlen(AUTH_TOKEN) == 0) return true;
  if (!server.hasHeader("X-Claudy-Token")) return false;
  return server.header("X-Claudy-Token") == AUTH_TOKEN;
}

static void copyStr(char* dst, size_t cap, const char* src) {
  if (!src) { dst[0] = 0; return; }
  strncpy(dst, src, cap - 1);
  dst[cap - 1] = 0;
}

static void handleState() {
  if (!authOk()) { server.send(401, "text/plain", "auth"); return; }

  String body = server.arg("plain");
  if (body.length() == 0) { server.send(400, "text/plain", "empty body"); return; }

  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) { server.send(400, "text/plain", err.c_str()); return; }

  const char* st   = doc["state"]   | "";
  const char* tool = doc["tool"]    | "";
  const char* msg  = doc["message"] | "";
  uint32_t used    = doc["tokens"]["used"] | 0;
  uint32_t maxv    = doc["tokens"]["max"]  | 0;

  if (lvgl_port_lock(200)) {
    g_state.state = parseStateName(st);
    g_state.tool  = parseToolName(tool);
    copyStr(g_state.message, sizeof(g_state.message), msg);
    if (maxv > 0) {
      g_state.tokensUsed = used;
      g_state.tokensMax  = maxv;
    }
    g_state.lastUpdateMs = millis();
    ui_apply_state();
    lvgl_port_unlock();
  }

  server.send(200, "application/json", "{\"ok\":true}");
}

static void handleGet() {
  StaticJsonDocument<256> doc;
  doc["state"]   = stateName(g_state.state);
  doc["tool"]    = toolName(g_state.tool);
  doc["message"] = g_state.message;
  doc["tokens"]["used"] = g_state.tokensUsed;
  doc["tokens"]["max"]  = g_state.tokensMax;
  doc["uptime_ms"]      = millis();
  doc["free_heap"]      = ESP.getFreeHeap();
  doc["ip"] = WiFi.localIP().toString();
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

static void handleRoot() {
  String html = "<!doctype html><meta charset=utf-8><title>Claudy-C6</title>"
                "<style>body{font-family:-apple-system,sans-serif;background:#111;color:#eee;padding:24px;max-width:600px;margin:auto}"
                "code{background:#222;padding:2px 6px;border-radius:4px}</style>"
                "<h1>Claudy (ESP32-C6)</h1><p>State: <b>";
  html += stateName(g_state.state);
  html += "</b></p><p>Tool: ";
  html += toolName(g_state.tool);
  html += "</p><p>Message: ";
  html += g_state.message;
  html += "</p><pre>curl -X POST http://";
  html += MDNS_HOSTNAME;
  html += ".local/state -H 'Content-Type: application/json' \\\n  -d '{\"state\":\"thinking\",\"message\":\"hello\"}'</pre>";
  server.send(200, "text/html", html);
}

bool netBegin() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(MDNS_HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("WiFi: connecting to %s\n", WIFI_SSID);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi: connect failed");
    s_connected = false;
    return false;
  }
  WiFi.setSleep(false);
  Serial.printf("WiFi: connected, IP=%s\n", WiFi.localIP().toString().c_str());

  if (MDNS.begin(MDNS_HOSTNAME)) {
    MDNS.addService("http", "tcp", HTTP_PORT);
    Serial.printf("mDNS: http://%s.local/\n", MDNS_HOSTNAME);
  } else {
    Serial.println("mDNS: failed to start");
  }

  const char* hdrKeys[] = {"X-Claudy-Token"};
  server.collectHeaders(hdrKeys, 1);
  server.on("/",       HTTP_GET,  handleRoot);
  server.on("/state",  HTTP_GET,  handleGet);
  server.on("/state",  HTTP_POST, handleState);
  server.onNotFound([]() { server.send(404, "text/plain", "not found"); });
  server.begin();
  Serial.printf("HTTP: listening on :%d\n", HTTP_PORT);

  s_connected = true;
  return true;
}

void netLoop() {
  server.handleClient();
}

bool netIsConnected() {
  return s_connected && WiFi.status() == WL_CONNECTED;
}
```

- [ ] **Step 9.3: Rewrite `firmware-c6.ino` for production loop**

```c
#include <Arduino.h>
#include "lvgl.h"
#include "config.h"
#include "state.h"
#include "hw/i2c_bus.h"
#include "hw/pmic_axp2101.h"
#include "hw/co5300.h"
#include "hw/touch_cst9220.h"
#include "hw/pins.h"
#include "ui/lvgl_port.h"
#include "ui/ui.h"
#include "ui/theme.h"
#include "net/net.h"

AppState g_state;
esp_lcd_panel_io_handle_t g_io;
esp_lcd_panel_handle_t    g_panel;

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n=== Claudy-C6 boot ===");

  i2c_bus_begin();
  if (!pmic_init())                              while (true) delay(1000);
  delay(100);
  if (!co5300_init(&g_io, &g_panel))             while (true) delay(1000);
  esp_lcd_panel_disp_on_off(g_panel, true);
  co5300_set_brightness(g_io, BRIGHTNESS * 255 / 100);

  if (!lvgl_port_init(g_io, g_panel))            while (true) delay(1000);
  touch_init();

  if (lvgl_port_lock(200)) {
    ui_init();
    g_state.state = STATE_BOOT;
    strncpy(g_state.message, "Connecting WiFi...", sizeof(g_state.message));
    ui_apply_state();
    lvgl_port_unlock();
  }

  if (netBegin()) {
    if (lvgl_port_lock(200)) {
      g_state.state = STATE_IDLE;
      snprintf(g_state.message, sizeof(g_state.message),
               "%s.local  %s", MDNS_HOSTNAME, WiFi.localIP().toString().c_str());
      ui_apply_state();
      lvgl_port_unlock();
    }
  } else {
    if (lvgl_port_lock(200)) {
      g_state.state = STATE_ERROR;
      strncpy(g_state.message, "WiFi failed. Check config.h", sizeof(g_state.message));
      ui_apply_state();
      lvgl_port_unlock();
    }
  }

  Serial.printf("Free heap after setup: %u bytes\n", ESP.getFreeHeap());
}

void loop() {
  netLoop();

  // Idle timeout
  if (IDLE_TIMEOUT_MS > 0 &&
      g_state.state != STATE_IDLE &&
      g_state.state != STATE_BOOT &&
      g_state.state != STATE_ERROR &&
      g_state.lastUpdateMs > 0 &&
      millis() - g_state.lastUpdateMs > IDLE_TIMEOUT_MS) {
    if (lvgl_port_lock(100)) {
      g_state.state = STATE_IDLE;
      g_state.tool = TOOL_NONE;
      strncpy(g_state.message, "Idle", sizeof(g_state.message));
      ui_apply_state();
      lvgl_port_unlock();
    }
  }

  uint16_t tx, ty;
  if (touch_read(&tx, &ty)) {
    // Tap-to-wake: cancel idle by refreshing lastUpdateMs and bumping brightness.
    co5300_set_brightness(g_io, BRIGHTNESS * 255 / 100);
    g_state.lastUpdateMs = millis();
  }

  delay(lvgl_port_tick());
}
```

- [ ] **Step 9.4: Set real WiFi credentials**

Edit `firmware-c6/config.h` and set `WIFI_SSID` + `WIFI_PASSWORD` to real values.

- [ ] **Step 9.5: Build, flash, verify (Gate G7)**

```bash
./scripts/build-c6.sh && ./scripts/flash-c6.sh && ./scripts/monitor-c6.sh
```

Expected serial:

```
WiFi: connecting to <yourssid>
.......
WiFi: connected, IP=192.168.X.Y
mDNS: http://claudy.local/
HTTP: listening on :80
Free heap after setup: <number>
```

On your Mac:

```bash
ping -c 3 claudy.local
curl http://claudy.local/state
curl -X POST http://claudy.local/state \
  -H 'Content-Type: application/json' \
  -d '{"state":"working","tool":"Edit","message":"firmware-c6/firmware-c6.ino","tokens":{"used":85000,"max":200000}}'
```

**Gate G7 passes if:**
- `ping` resolves and responds
- `GET /state` returns JSON with current state
- `POST /state` returns `{"ok":true}` and the screen immediately updates to Working / Edit chip / "firmware-c6.ino" message / token bar at 42%

If `claudy.local` doesn't resolve, use the IP from serial output directly.

If the existing T-Display-S3 device is on the same network with the same hostname, **flash one at a time** during testing — both will fight for the name.

- [ ] **Step 9.6: Commit**

```bash
git add firmware-c6/net/ firmware-c6/firmware-c6.ino
git commit -m "firmware-c6: WiFi + mDNS + /state API; G7 passes (curl drives screen)"
```

---

## Task 10: End-to-end with Claude Code Hooks (Gate G8 + G9)

**Files:** No new files. Verification only.

**Goal:** Real Claude Code hook events reach the device, and the screen reacts in < 200ms. All 6 states render correctly from real events.

- [ ] **Step 10.1: Verify all 6 states with `scripts/test-state.sh`**

The existing test script POSTs sample states. Run it pointed at the C6 device:

```bash
./scripts/test-state.sh
```

It should hit `http://claudy.local/state` and cycle through all states. Visually confirm each state renders correctly. **This is Gate G8.**

- [ ] **Step 10.2: Optional — install hooks for the live device**

If the user wants to use the C6 device with Claude Code:

```bash
./bridge/install-hooks.sh
```

(If both boards are flashed and online, set `CLAUDY_URL` in `~/.zshenv` to the IP of the specific board you want updates on.)

- [ ] **Step 10.3: Exercise end-to-end (Gate G9)**

Open a Claude Code session, run `ls` (Bash tool), edit a file (Edit tool), ask a question. Watch the C6 screen.

**Gate G9 passes if:**
- Each tool call appears on the screen with the corresponding chip
- Reaction latency feels < 200ms (subjective)
- The mascot animations look right (working = energetic, waiting = head-tilt, etc.)
- Token bar updates after long Read/Edit chains

- [ ] **Step 10.4: Commit** (no code changes — record verification in commit)

If you made any tweaks in this task (e.g., font size adjustments, color tuning):

```bash
git add firmware-c6/
git commit -m "firmware-c6: end-to-end with Claude Code hooks verified (G8 + G9 pass)"
```

---

## Task 11: Idle Fade + Soak Test (Gate G10)

**Files:**
- Modify: `firmware-c6/firmware-c6.ino`
- Modify: `firmware-c6/hw/pmic_axp2101.cpp` (or `co5300.cpp`) to actually implement brightness fade

**Goal:** After 60s of inactivity, brightness fades to low (≈ 20%). A tap (or new POST) restores full brightness.

- [ ] **Step 11.1: Implement real brightness control**

Update `firmware-c6/hw/co5300.cpp`'s `co5300_set_brightness` to send the actual DCS 0x51 command (already done in Task 4). Confirm via serial: print current brightness after each change.

- [ ] **Step 11.2: Add fade logic in `firmware-c6.ino`**

Replace the simple idle timeout block in `loop()` with a graduated fade:

```c
  // Brightness fade: 0..IDLE_TIMEOUT_MS = full; after that, ramp down to 20%
  // over the next 5s. Touch / POST resets the timer.
  static uint32_t last_set_brightness = 255;
  static uint32_t last_active = 0;
  if (g_state.lastUpdateMs > last_active) last_active = g_state.lastUpdateMs;

  uint32_t since = millis() - last_active;
  uint32_t target;
  if (since < IDLE_TIMEOUT_MS) {
    target = BRIGHTNESS * 255 / 100;
  } else if (since < IDLE_TIMEOUT_MS + 5000) {
    // Linear fade to 20% of full over 5s
    uint32_t full = BRIGHTNESS * 255 / 100;
    uint32_t low  = full * 20 / 100;
    uint32_t span = full - low;
    target = full - span * (since - IDLE_TIMEOUT_MS) / 5000;
  } else {
    target = (BRIGHTNESS * 255 / 100) * 20 / 100;
  }

  if (target != last_set_brightness) {
    co5300_set_brightness(g_io, target);
    last_set_brightness = target;
  }
```

And on touch tap (already in Task 9 step 9.3), set `last_active = millis()` to wake.

- [ ] **Step 11.3: Build, flash, idle-test**

Send a state update, then wait. Brightness should fade after 60s. Tap the screen → instant full brightness.

- [ ] **Step 11.4: Soak test (Gate G10)**

Leave the device running overnight (24h). Before leaving, record `GET /state` `free_heap` value. After 24h, check again:

- Device still responds to HTTP requests
- `free_heap` has not dropped (allow ±5KB jitter)
- No serial crashes / panics in monitor (if connected)
- Tap-to-wake still works

**Gate G10 passes when 24h pass without regression.**

- [ ] **Step 11.5: Commit**

```bash
git add firmware-c6/firmware-c6.ino firmware-c6/hw/co5300.cpp
git commit -m "firmware-c6: idle fade + tap-to-wake; G10 soak test passes"
```

---

## Task 12: Documentation

**Files:**
- Create: `firmware-c6/README.md`
- Modify: `README.md`
- Modify: `README.zh-TW.md`

**Goal:** Discoverable from the root README. C6-specific quickstart lives in `firmware-c6/README.md`.

- [ ] **Step 12.1: Create `firmware-c6/README.md`**

```markdown
# Claudy on ESP32-C6 AMOLED 2.16

This subdirectory targets the **Waveshare ESP32-C6 Touch AMOLED 2.16** board.
The original LilyGo T-Display-S3 firmware in `firmware/` is unchanged.

The HTTP API, mDNS hostname (`claudy.local`), and the Mac bridge are identical
between the two boards — you can swap hardware without touching `bridge/`.

## Hardware

- Waveshare ESP32-C6-Touch-AMOLED-2.16 (480×480 AMOLED, CO5300, CST9220 touch)
- USB-C cable

## Differences from T-Display-S3 firmware

- Uses `esp_lcd` + LVGL 8 (LovyanGFX has no CO5300 driver)
- Single-core RISC-V, no PSRAM — mascot canvas is bounded to fit DRAM
- Tap-to-wake gesture: tapping the screen restores brightness from idle fade

## Quickstart (Mac)

```bash
cd ~/Developer/Claudy

# 1. Toolchain (one-time, installs LVGL too)
./scripts/setup.sh

# 2. WiFi
cp firmware-c6/config.h.example firmware-c6/config.h
$EDITOR firmware-c6/config.h           # set WIFI_SSID + WIFI_PASSWORD

# 3. Build & flash
./scripts/build-c6.sh
./scripts/flash-c6.sh                  # auto-detects /dev/cu.usbmodem*

# 4. Watch first boot
./scripts/monitor-c6.sh
# Expect: "WiFi: connected, IP=..." and "mDNS: http://claudy.local/"

# 5. Verify
curl http://claudy.local/state
./scripts/test-state.sh                # cycles all states

# 6. Hook into Claude Code (same as the S3 firmware)
./bridge/install-hooks.sh
```

## Troubleshooting

**No serial port at flash time**
Plug in USB-C. If still missing: hold **BOOT**, tap **RESET**, release **BOOT**.
The C6 uses native USB CDC — port appears as `/dev/cu.usbmodem*`.

**Black screen / no display**
The CO5300 only powers up after AXP2101 enables its rails. If you reordered
boot steps, that will fail silently. See setup() in `firmware-c6.ino` —
`pmic_init()` must precede `co5300_init()`.

**"mascot canvas alloc failed"**
Free heap too low. Reduce `MASCOT_W` and `MASCOT_H` in `firmware-c6/ui/theme.h`
(400×240 or 360×240).

**Colors look wrong (red/blue swapped)**
Toggle `LV_COLOR_16_SWAP` in `firmware-c6/lv_conf.h` and rebuild.

## Layout

```
firmware-c6/
  firmware-c6.ino    main: setup() / loop()
  state.h            AppState enums (shared semantics with firmware/state.h)
  lv_conf.h          LVGL config

  hw/                hardware drivers — one peripheral per file
    pins.h
    i2c_bus.{h,cpp}
    pmic_axp2101.{h,cpp}
    co5300.{h,cpp}            + co5300_vendor/ (Waveshare-provided init)
    touch_cst9220.{h,cpp}

  ui/                no hardware here — pure LVGL
    lvgl_port.{h,cpp}
    theme.h
    gfx.{h,cpp}
    mascot.{h,cpp}
    ui.{h,cpp}

  net/
    net.{h,cpp}      WiFi + mDNS + WebServer (parallel to firmware/net.cpp)
```

## License

Same as parent project. `hw/co5300_vendor/` is vendored from Waveshare's
official example repo (Apache-2.0 / per upstream LICENSE).
```

- [ ] **Step 12.2: Update root `README.md`**

Find the "Hardware" section (around line 32-35) and replace:

Find:

```
## Hardware

- LilyGo T-Display-S3 (ESP32-S3 with 8-bit parallel ST7789, 16MB flash + PSRAM)
- USB-C cable
```

Replace with:

```
## Hardware

Two supported boards (each in its own firmware tree):

- **LilyGo T-Display-S3** (ESP32-S3, 170×320 ST7789) — `firmware/`, the default. Steps below.
- **Waveshare ESP32-C6 Touch AMOLED 2.16** (ESP32-C6, 480×480 AMOLED, touch) — see [`firmware-c6/README.md`](firmware-c6/README.md).

The Mac bridge and HTTP API are identical for both — pick one and follow its quickstart.
```

- [ ] **Step 12.3: Update `README.zh-TW.md` analogously**

Read `README.zh-TW.md`, find the "硬體" section, apply the same dual-board change in Traditional Chinese:

```
## 硬體

支援兩塊開發板(各自獨立的韌體目錄):

- **LilyGo T-Display-S3**(ESP32-S3, 170×320 ST7789)— `firmware/`,預設,以下流程
- **Waveshare ESP32-C6 Touch AMOLED 2.16**(ESP32-C6, 480×480 AMOLED, 觸控)— 見 [`firmware-c6/README.md`](firmware-c6/README.md)

Mac bridge 和 HTTP API 兩塊板都一樣 — 選一塊照各自流程做即可。
```

- [ ] **Step 12.4: Commit**

```bash
git add firmware-c6/README.md README.md README.zh-TW.md
git commit -m "docs: ESP32-C6 board quickstart + dual-board readme nav"
```

---

## Verification Checklist (final)

After all tasks complete:

- [ ] `firmware/` still builds (`./scripts/build.sh`) and flashes — original board unaffected
- [ ] `firmware-c6/` builds (`./scripts/build-c6.sh`) cleanly with no warnings
- [ ] All 10 bring-up gates passed during their respective tasks
- [ ] `git log --oneline` shows ~12 commits, one per task
- [ ] `firmware-c6/README.md` quickstart instructions work end-to-end on a fresh shell
- [ ] Free heap reported in serial after `setup()` is ≥ 30KB
- [ ] `GET /state` returns JSON with `free_heap` (added in Task 9 helps soak monitoring)
