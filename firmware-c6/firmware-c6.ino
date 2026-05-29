#include <Arduino.h>
#include "lvgl.h"
#include "state.h"
#include "src/hw/i2c_bus.h"
#include "src/hw/pmic_axp2101.h"
#include "src/hw/co5300.h"
#include "src/hw/touch_cst9220.h"
#include "src/hw/pins.h"
#include "src/ui/lvgl_port.h"
#include "src/ui/ui.h"
#include "src/ui/theme.h"

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
  "Editing firmware-c6/src/ui/ui.cpp",
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
