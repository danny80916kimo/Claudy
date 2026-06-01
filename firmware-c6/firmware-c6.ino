#include <Arduino.h>
#include "config.h"
#ifdef CLAUDY_TRANSPORT_WIFI
#include <WiFi.h>
#endif // CLAUDY_TRANSPORT_WIFI
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
#include "src/net/net.h"
#ifdef CLAUDY_TRANSPORT_BLE
#include "src/net/ble.h"
#endif

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
    strncpy(g_state.message, "Starting...", sizeof(g_state.message));
    ui_apply_state();
    lvgl_port_unlock();
  }

#ifdef CLAUDY_TRANSPORT_WIFI
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
#endif // CLAUDY_TRANSPORT_WIFI

#ifdef CLAUDY_TRANSPORT_BLE
  bleBegin();
  if (lvgl_port_lock(200)) {
    g_state.state = STATE_IDLE;
    strncpy(g_state.message, "Bluetooth ready", sizeof(g_state.message));
    ui_apply_state();
    lvgl_port_unlock();
  }
#endif

  Serial.printf("Free heap after setup: %u bytes\n", ESP.getFreeHeap());
}

void loop() {
#ifdef CLAUDY_TRANSPORT_WIFI
  netLoop();
#endif // CLAUDY_TRANSPORT_WIFI

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
