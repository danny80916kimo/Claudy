#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "state.h"
#include "display.h"
#include "mascot.h"
#include "net.h"

AppState g_state;

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n=== Claudy boot ===");

  displayBegin(BRIGHTNESS);
  g_state.state = STATE_BOOT;
  strncpy(g_state.message, "Connecting WiFi...", sizeof(g_state.message));
  requestRedraw();
  renderFrame();

  if (netBegin()) {
    g_state.state = STATE_IDLE;
    snprintf(g_state.message, sizeof(g_state.message),
             "%s.local  %s", MDNS_HOSTNAME, WiFi.localIP().toString().c_str());
  } else {
    g_state.state = STATE_ERROR;
    strncpy(g_state.message, "WiFi failed. Check config.h", sizeof(g_state.message));
  }
  requestRedraw();
}

void loop() {
  netLoop();

  // Auto-return to IDLE after timeout
  if (IDLE_TIMEOUT_MS > 0 &&
      g_state.state != STATE_IDLE &&
      g_state.state != STATE_BOOT &&
      g_state.state != STATE_ERROR &&
      g_state.lastUpdateMs > 0 &&
      millis() - g_state.lastUpdateMs > IDLE_TIMEOUT_MS) {
    g_state.state = STATE_IDLE;
    g_state.tool = TOOL_NONE;
    strncpy(g_state.message, "Idle", sizeof(g_state.message));
    requestRedraw();
  }

  // Render only when there's something to draw (state changed via requestRedraw,
  // or the current state's animation tick has elapsed). This frees CPU/bus time
  // for the HTTP server during steady-state operation.
  static unsigned long lastFrame = 0;
  static MascotState   lastAnimState = STATE_BOOT;
  unsigned long now = millis();
  uint32_t interval = mascotAnimInterval(g_state.state);

  if (g_state.state != lastAnimState) {
    lastAnimState = g_state.state;
    lastFrame = 0;   // force immediate render of new state
  }

  if (interval > 0 && now - lastFrame >= interval) {
    lastFrame = now;
    requestRedraw();
  }

  renderFrame();    // no-op unless dirty
  netLoop();        // extra drain in case the render took a moment
}
