#include "../../config.h"
#ifdef CLAUDY_TRANSPORT_BLE

#include "ble.h"
#include "../../state.h"
#include "../ui/ui.h"
#include "../ui/lvgl_port.h"

#include <NimBLEDevice.h>
#include <ArduinoJson.h>

extern AppState g_state;

#define BLE_SERVICE_UUID "c1a0dc00-1f9b-4e3a-9b00-a1b2c3d4e5f6"
#define BLE_STATE_UUID   "c1a0dc01-1f9b-4e3a-9b00-a1b2c3d4e5f6"

static bool s_connected = false;

// Apply one state-JSON payload to g_state + UI (mirror of net.cpp handleState).
static void applyStateJson(const char* body, size_t len) {
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, body, len)) return;   // ignore malformed

  const char* st   = doc["state"]   | "";
  const char* tool = doc["tool"]    | "";
  const char* msg  = doc["message"] | "";
  uint32_t used    = doc["tokens"]["used"] | 0;
  uint32_t maxv    = doc["tokens"]["max"]  | 0;

  if (lvgl_port_lock(200)) {
    g_state.state = parseStateName(st);
    g_state.tool  = parseToolName(tool);
    strncpy(g_state.message, msg, sizeof(g_state.message) - 1);
    g_state.message[sizeof(g_state.message) - 1] = 0;
    if (maxv > 0) {
      g_state.tokensUsed = used;
      g_state.tokensMax  = maxv;
    }
    g_state.lastUpdateMs = millis();
    ui_apply_state();
    lvgl_port_unlock();
  }
}

class StateCallbacks : public NimBLECharacteristicCallbacks {
  // NimBLE 2.x signature. If the installed lib is 1.x, drop the 2nd param.
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& connInfo) override {
    NimBLEAttValue v = c->getValue();
    applyStateJson((const char*)v.data(), v.length());
  }
};

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* s, NimBLEConnInfo& info) override { s_connected = true; }
  void onDisconnect(NimBLEServer* s, NimBLEConnInfo& info, int reason) override {
    s_connected = false;
    NimBLEDevice::getAdvertising()->start();   // allow the daemon to reconnect
  }
};

bool bleBegin() {
  NimBLEDevice::init(BLE_DEVICE_NAME);
  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  NimBLEService* svc = server->createService(BLE_SERVICE_UUID);
  NimBLECharacteristic* ch = svc->createCharacteristic(
      BLE_STATE_UUID,
      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  ch->setCallbacks(new StateCallbacks());
  svc->start();

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(BLE_SERVICE_UUID);
  adv->setName(BLE_DEVICE_NAME);
  adv->start();

  Serial.printf("BLE: advertising as \"%s\"\n", BLE_DEVICE_NAME);
  return true;
}

bool bleConnected() { return s_connected; }

#endif // CLAUDY_TRANSPORT_BLE
