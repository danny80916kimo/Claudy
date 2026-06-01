# BLE Transport Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Drive the battery-powered ESP32-C6 Claudy device from Claude Code hooks over Bluetooth LE (no WiFi), via a NimBLE GATT server on the device and a persistent `bleak`-based macOS daemon, with the existing WiFi path preserved behind a compile-time switch.

**Architecture:** A compile-time `CLAUDY_TRANSPORT_BLE`/`_WIFI` selector in `config.h` chooses which transport is linked (flash budget — can't fit both). BLE build: a NimBLE GATT server exposes one writable "state" characteristic whose on-write callback reuses the exact JSON→`g_state` parse path. On the Mac, `send_state.py` writes hook events to a unix socket; a background `claudy-bled.py` daemon (asyncio + bleak) holds the BLE link and forwards them, auto-reconnecting. Fire-and-forget semantics throughout — never block Claude.

**Tech Stack:** ESP32-C6 / arduino-esp32 3.0.7, NimBLE-Arduino, ArduinoJson 7; Python 3 (stdlib `socket`/`asyncio` + `bleak`), macOS LaunchAgent.

**Design doc:** `docs/superpowers/specs/2026-06-01-ble-transport-design.md`. Read it before Task 1.

**Fixed UUIDs (identical in firmware + daemon):**
- Service:    `c1a0dc00-1f9b-4e3a-9b00-a1b2c3d4e5f6`
- State char: `c1a0dc01-1f9b-4e3a-9b00-a1b2c3d4e5f6`

---

## File Structure (locked)

```
firmware-c6/
  config.h.example         # + transport selector + BLE device name
  config.h                 # (gitignored) user sets selector
  sketch.yaml              # + NimBLE-Arduino library
  src/net/
    net.h                  # unchanged
    net.cpp                # body wrapped in #ifdef CLAUDY_TRANSPORT_WIFI
    ble.h                  # NEW: bleBegin(), bleConnected()
    ble.cpp                # NEW: NimBLE GATT server, #ifdef CLAUDY_TRANSPORT_BLE
  firmware-c6.ino          # #ifdef-selects transport in setup()/loop()

bridge/
  send_state.py            # + send() dispatch: wifi=HTTP (existing), ble=unix socket
  claudy-bled.py           # NEW: bleak daemon (BLE link + unix socket server)
  install-ble-daemon.sh    # NEW: venv + bleak + LaunchAgent
  uninstall-ble-daemon.sh  # NEW
  tests/test_send_ble.py   # NEW: pytest for the BLE socket send path

docs / READMEs updated in Task 7.
```

**Boundaries:** `ble.cpp` owns all NimBLE; it shares only the JSON-parse helper with the rest. `claudy-bled.py` owns the BLE link + socket; `send_state.py` only knows "write a line to the socket." The daemon and firmware agree solely on the two UUIDs and the JSON shape.

---

## Task 1: Compile-time transport switch (WiFi build unchanged)

Decouple the transport before adding BLE. After this task the firmware still builds and behaves exactly as today in WiFi mode; nothing BLE yet.

**Files:**
- Modify: `firmware-c6/config.h.example`
- Modify: `firmware-c6/config.h` (gitignored — edit in place)
- Modify: `firmware-c6/src/net/net.cpp`
- Modify: `firmware-c6/firmware-c6.ino`

- [ ] **Step 1.1: Add the selector to `config.h.example`**

Add after the `IDLE_TIMEOUT_MS` line:

```c

// Transport: define exactly ONE. BLE is for the battery-powered C6 with no WiFi.
// #define CLAUDY_TRANSPORT_WIFI
#define CLAUDY_TRANSPORT_BLE

// BLE advertised device name (must match CLAUDY_BLE_NAME in the Mac daemon).
#define BLE_DEVICE_NAME  "claudy"
```

- [ ] **Step 1.2: Add the same lines to `config.h`**

Append the identical block to `firmware-c6/config.h` (the live, gitignored file). For now the user's board uses BLE, so leave `CLAUDY_TRANSPORT_BLE` defined and `CLAUDY_TRANSPORT_WIFI` commented.

- [ ] **Step 1.3: Guard `net.cpp` so it compiles to nothing in BLE builds**

At the very top of `firmware-c6/src/net/net.cpp` (line 1, before `#include "net.h"`), add:

```c
#include "../../config.h"
#ifdef CLAUDY_TRANSPORT_WIFI
```

At the very end of the file (after the final `}` of `netIsConnected()`), add:

```c
#endif // CLAUDY_TRANSPORT_WIFI
```

(The existing `#include "../../config.h"` further down is now redundant but harmless — leave it; it's inside the guard.)

- [ ] **Step 1.4: Make `net.h` safe to include in either build**

`net.h` only declares functions, so it's fine to include always. No change needed. (The `.ino` will guard the *calls*, not the include.)

- [ ] **Step 1.5: Guard the transport calls in `firmware-c6.ino`**

In `firmware-c6/firmware-c6.ino`, the include line `#include "src/net/net.h"` stays.

Replace the WiFi bring-up block in `setup()` — currently:

```c
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
```

with:

```c
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
#endif
```

In `loop()`, change the first line from `netLoop();` to:

```c
#ifdef CLAUDY_TRANSPORT_WIFI
  netLoop();
#endif
```

Also wrap the `#include "src/net/net.h"` and `#include "lvgl.h"`... leave includes as-is. Only `WiFi.h` matters: the `.ino` references `WiFi.localIP()` only inside the `#ifdef CLAUDY_TRANSPORT_WIFI` block now, but the implementer added `#include <WiFi.h>` at the top in Task 9. That include is harmless in a BLE build (WiFi lib still compiles, just unused) — but to actually free the WiFi flash we must not *link* WiFi. Guard the include too:

Change `#include <WiFi.h>` to:

```c
#ifdef CLAUDY_TRANSPORT_WIFI
#include <WiFi.h>
#endif
```

- [ ] **Step 1.6: Verify the WiFi build still works (temporary)**

Temporarily flip `config.h` to WiFi to prove the guards didn't break the existing path:

```bash
# temporarily: in config.h comment CLAUDY_TRANSPORT_BLE, uncomment CLAUDY_TRANSPORT_WIFI
./scripts/build-c6.sh
```

Expected: compiles, flash ~91% (same as before — WiFi still linked). Then set `config.h` back to BLE (`CLAUDY_TRANSPORT_BLE` defined, `_WIFI` commented) and build again:

```bash
./scripts/build-c6.sh
```

Expected: compiles. With BLE selected and `net.cpp` guarded out and `WiFi.h` not included, the WiFi/WebServer code is dropped — **flash should DROP below 91%** (freeing room for NimBLE in Task 2). Record the BLE-mode flash %.

> If the BLE-mode build errors with undefined `netBegin`/`netLoop`/`netIsConnected`: those are only *called* inside `#ifdef CLAUDY_TRANSPORT_WIFI`, so there should be no references. If `netIsConnected()` is referenced elsewhere, guard that call too.

- [ ] **Step 1.7: Commit**

```bash
git add firmware-c6/config.h.example firmware-c6/src/net/net.cpp firmware-c6/firmware-c6.ino
git commit -m "firmware-c6: compile-time transport switch (WiFi guarded behind ifdef)"
```

(Do NOT commit `config.h` — gitignored.)

---

## Task 2: NimBLE GATT server + flash gate + GB1

**Files:**
- Modify: `firmware-c6/sketch.yaml`
- Create: `firmware-c6/src/net/ble.h`
- Create: `firmware-c6/src/net/ble.cpp`
- Modify: `firmware-c6/firmware-c6.ino`

**Goal:** Device advertises as `claudy` over BLE; writing JSON to the state characteristic updates the screen. The build fits flash.

> **NimBLE version note:** arduino-esp32 3.0.7 needs **NimBLE-Arduino 2.x** (the 1.x line is for arduino-esp32 2.x / IDF4 and will not compile here). The 2.x `onWrite` callback signature is `onWrite(NimBLECharacteristic*, NimBLEConnInfo&)`. If the installed version rejects that signature, the only likely alternative is the 1.x form `onWrite(NimBLECharacteristic*)` — try that. Do not change libraries.

- [ ] **Step 2.1: Add NimBLE to `sketch.yaml`**

In `firmware-c6/sketch.yaml`, add to the `libraries:` list so it reads:

```yaml
    libraries:
      - lvgl (8.3.11)
      - ArduinoJson (7.1.0)
      - NimBLE-Arduino (2.2.3)
```

(If `arduino-cli` reports 2.2.3 unavailable, use the latest 2.x it lists: `arduino-cli lib search NimBLE-Arduino`. Pin whatever 2.x version resolves and note it in the commit.)

- [ ] **Step 2.2: Create `firmware-c6/src/net/ble.h`**

```c
#pragma once
#include <Arduino.h>

// Start the NimBLE GATT server + advertising. Returns true on success.
bool bleBegin();

// Is a central (the Mac daemon) currently connected?
bool bleConnected();
```

- [ ] **Step 2.3: Create `firmware-c6/src/net/ble.cpp`**

```c
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
```

> **API caveats to expect (NimBLE 2.x):** `NimBLEAttValue` has `.data()`/`.length()`. `NIMBLE_PROPERTY::WRITE_NR` is write-no-response. `adv->setName(...)` exists in 2.x; if not, use `NimBLEDevice::init(BLE_DEVICE_NAME)` alone (the init name is advertised). If `onDisconnect` signature differs, match the header in `NimBLEServerCallbacks`.

- [ ] **Step 2.4: Wire BLE into `firmware-c6.ino`**

Add near the other `src/net` include:

```c
#ifdef CLAUDY_TRANSPORT_BLE
#include "src/net/ble.h"
#endif
```

(Place it after `#include "src/net/net.h"`. The `config.h` include is already at the top of the .ino, so `CLAUDY_TRANSPORT_BLE` is defined here.)

In `setup()`, immediately AFTER the `#endif` that closes the `CLAUDY_TRANSPORT_WIFI` bring-up block (from Task 1 Step 1.5), add:

```c
#ifdef CLAUDY_TRANSPORT_BLE
  bleBegin();
  if (lvgl_port_lock(200)) {
    g_state.state = STATE_IDLE;
    strncpy(g_state.message, "Bluetooth ready", sizeof(g_state.message));
    ui_apply_state();
    lvgl_port_unlock();
  }
#endif
```

No `loop()` change needed for BLE (NimBLE runs in its own task).

- [ ] **Step 2.5: Build + FLASH GATE**

Confirm `config.h` has `CLAUDY_TRANSPORT_BLE` defined (and `_WIFI` commented), then:

```bash
./scripts/build-c6.sh
```

**Flash gate:** must report **< 100%** (ideally < 95%). Record the %.
- If it FITS: proceed.
- If it OVERFLOWS: do NOT shrink the font further. Switch to a custom partition that reclaims the unused FAT:
  1. Create `firmware-c6/partitions.csv`:
     ```
     # Name,   Type, SubType, Offset,  Size
     nvs,      data, nvs,     0x9000,  0x5000
     otadata,  data, ota,     0xe000,  0x2000
     app0,     app,  ota_0,   0x10000, 0x600000
     spiffs,   data, spiffs,  0x610000,0x9E0000
     coredump, data, coredump,0xFF0000,0x10000
     ```
     (6MB app, rest spiffs — uses the 16MB flash fully.)
  2. In `sketch.yaml` and `scripts/build-c6.sh` + `scripts/flash-c6.sh`, change `PartitionScheme=app3M_fat9M_16MB` to `PartitionScheme=custom`. arduino-cli picks up `partitions.csv` from the sketch dir for `custom`.
  3. Rebuild; flash % should now be ~40%.
  Commit the partition change as part of this task.

- [ ] **Step 2.6: GB1 — hardware verify (USER)**

Flash and open a BLE scanner (phone "nRF Connect", or `bleak`):

```bash
./scripts/flash-c6.sh && ./scripts/monitor-c6.sh
```

Serial should show `BLE: advertising as "claudy"`. In nRF Connect: find `claudy`, connect, find service `c1a0dc00-…`, write to characteristic `c1a0dc01-…` the bytes of:

`{"state":"working","tool":"Edit","message":"BLE 測試","tokens":{"used":85000,"max":200000}}`

(Write as a UTF-8 / text value.) **GB1 passes if the screen changes to Working / Edit / "BLE 測試" / 42%.**

Quick scriptable alternative (no phone), once Task 4's deps exist — skip for now; nRF Connect is fine.

- [ ] **Step 2.7: Commit**

```bash
git add firmware-c6/sketch.yaml firmware-c6/src/net/ble.h firmware-c6/src/net/ble.cpp firmware-c6/firmware-c6.ino
# include partitions.csv + scripts if the flash gate required them
git commit -m "firmware-c6: NimBLE GATT state server; GB1 (BLE write drives screen)"
```

---

## Task 3: send_state.py BLE branch (unix socket) + unit test

**Files:**
- Modify: `bridge/send_state.py`
- Create: `bridge/tests/test_send_ble.py`

**Goal:** In `CLAUDY_TRANSPORT=ble` mode, `send_state.py` writes the payload as one JSON line to `~/.claudy/ble.sock` and never raises if the socket is absent. WiFi mode is unchanged (default).

- [ ] **Step 3.1: Write the failing test**

Create `bridge/tests/test_send_ble.py`:

```python
import json, os, socket, tempfile, threading, importlib, sys

def _load_module(sock_path, transport):
    os.environ["CLAUDY_TRANSPORT"] = transport
    os.environ["CLAUDY_BLE_SOCK"] = sock_path
    sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
    import send_state
    importlib.reload(send_state)
    return send_state

def test_send_ble_writes_one_json_line_to_socket(tmp_path):
    sock_path = str(tmp_path / "ble.sock")
    received = []

    srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    srv.bind(sock_path)
    srv.listen(1)

    def accept():
        conn, _ = srv.accept()
        data = conn.recv(4096)
        received.append(data)
        conn.close()
    t = threading.Thread(target=accept); t.start()

    m = _load_module(sock_path, "ble")
    m.send({"state": "working", "message": "hi"})
    t.join(timeout=2.0); srv.close()

    assert received, "daemon socket got nothing"
    line = received[0].decode().strip()
    assert json.loads(line)["state"] == "working"

def test_send_ble_silent_when_socket_missing(tmp_path):
    sock_path = str(tmp_path / "nope.sock")  # no server listening
    m = _load_module(sock_path, "ble")
    m.send({"state": "idle"})   # must NOT raise
```

- [ ] **Step 3.2: Run it, watch it fail**

```bash
cd bridge && python3 -m pytest tests/test_send_ble.py -v
```

Expected: FAIL — `send_state` has no `send()` and ignores `CLAUDY_BLE_SOCK`.

- [ ] **Step 3.3: Implement the BLE branch in `send_state.py`**

Add near the other env config (after the `IP_CACHE_TTL` line, ~line 26):

```python
TRANSPORT  = os.environ.get("CLAUDY_TRANSPORT", "wifi")
BLE_SOCK   = os.environ.get("CLAUDY_BLE_SOCK", os.path.expanduser("~/.claudy/ble.sock"))
```

Add this function (next to `post`):

```python
def send_ble(payload):
    """BLE transport: hand the payload to the local daemon over a unix socket.
    Fire-and-forget — if the daemon isn't running, do nothing (never raise)."""
    import socket
    line = (json.dumps(payload) + "\n").encode()
    try:
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.settimeout(1.0)
        s.connect(BLE_SOCK)
        s.sendall(line)
        s.close()
    except Exception:
        pass


def send(payload):
    """Dispatch to the configured transport."""
    if TRANSPORT == "ble":
        send_ble(payload)
    else:
        post(payload)
```

In `main()`, change the two `post(...)` calls to `send(...)`:

```python
    send(payload)

    if state == "done":
        time.sleep(3)
        send({"state": "idle", "message": "Idle"})
```

- [ ] **Step 3.4: Run the test, watch it pass**

```bash
cd bridge && python3 -m pytest tests/test_send_ble.py -v
```

Expected: both tests PASS.

- [ ] **Step 3.5: Commit**

```bash
git add bridge/send_state.py bridge/tests/test_send_ble.py
git commit -m "bridge: CLAUDY_TRANSPORT=ble routes hook events to a unix socket"
```

---

## Task 4: claudy-bled.py daemon (bleak) + GB2

**Files:**
- Create: `bridge/claudy-bled.py`

**Goal:** A long-running process that holds the BLE link to the device and forwards socket lines to the state characteristic, reconnecting automatically.

- [ ] **Step 4.1: Create `bridge/claudy-bled.py`**

```python
#!/usr/bin/env python3
"""Claudy BLE daemon: hold a BLE link to the device and forward state-JSON
lines received on a unix socket to the device's state characteristic.

Run via the LaunchAgent from install-ble-daemon.sh (which provides bleak in a
venv). Env:
  CLAUDY_BLE_SOCK  unix socket path (default ~/.claudy/ble.sock)
  CLAUDY_BLE_NAME  advertised device name to match (default "claudy")
"""
import asyncio
import os
import sys

from bleak import BleakScanner, BleakClient

SERVICE_UUID = "c1a0dc00-1f9b-4e3a-9b00-a1b2c3d4e5f6"
CHAR_UUID    = "c1a0dc01-1f9b-4e3a-9b00-a1b2c3d4e5f6"
SOCK = os.environ.get("CLAUDY_BLE_SOCK", os.path.expanduser("~/.claudy/ble.sock"))
NAME = os.environ.get("CLAUDY_BLE_NAME", "claudy")

_latest = None          # most recent payload bytes (without newline)
_client = None          # connected BleakClient, or None


def log(msg):
    print(msg, flush=True)


async def _write(payload: bytes):
    c = _client
    if c is None or not c.is_connected:
        return
    try:
        await c.write_gatt_char(CHAR_UUID, payload, response=False)
    except Exception as e:
        log(f"write failed: {e}")


async def ble_link():
    """Maintain a connection to the device forever."""
    global _client
    backoff = 1
    while True:
        try:
            log(f"scanning for '{NAME}'...")
            dev = await BleakScanner.find_device_by_filter(
                lambda d, ad: (ad.local_name == NAME) or (d.name == NAME)
                or (SERVICE_UUID.lower() in [u.lower() for u in (ad.service_uuids or [])]),
                timeout=10.0,
            )
            if dev is None:
                await asyncio.sleep(backoff); backoff = min(backoff * 2, 5); continue
            log(f"connecting to {dev.address}...")
            async with BleakClient(dev) as c:
                _client = c
                backoff = 1
                log("connected")
                if _latest is not None:
                    await _write(_latest)
                while c.is_connected:
                    await asyncio.sleep(1)
        except Exception as e:
            log(f"link error: {e}")
        finally:
            _client = None
            log("disconnected")
            await asyncio.sleep(backoff); backoff = min(backoff * 2, 5)


async def handle_conn(reader, writer):
    global _latest
    try:
        data = await asyncio.wait_for(reader.read(8192), timeout=2.0)
    except Exception:
        writer.close(); return
    writer.close()
    line = data.decode("utf-8", errors="ignore").strip()
    if not line:
        return
    _latest = line.encode("utf-8")
    await _write(_latest)


async def main():
    os.makedirs(os.path.dirname(SOCK), exist_ok=True)
    try:
        os.remove(SOCK)
    except FileNotFoundError:
        pass
    server = await asyncio.start_unix_server(handle_conn, path=SOCK)
    log(f"listening on {SOCK}")
    await asyncio.gather(ble_link(), server.serve_forever())


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
```

```bash
chmod +x bridge/claudy-bled.py
```

- [ ] **Step 4.2: GB2 — run the daemon and forward a line (USER, needs bleak)**

This needs `bleak`. Quick manual venv to test before the installer exists:

```bash
python3 -m venv ~/.claudy/venv
~/.claudy/venv/bin/pip install bleak
~/.claudy/venv/bin/python bridge/claudy-bled.py
```

Expected log: `listening on ~/.claudy/ble.sock`, `scanning for 'claudy'...`, then `connected` once the device (flashed from Task 2) is powered and in range.

In another terminal, push a state:

```bash
printf '%s\n' '{"state":"done","message":"BLE 完成 — daemon ✓","tokens":{"used":120000,"max":200000}}' \
  | nc -U ~/.claudy/ble.sock
```

(If `nc -U` isn't available, use:
`python3 -c 'import socket,sys; s=socket.socket(socket.AF_UNIX);s.connect(sys.argv[1]);s.sendall(open(0,"rb").read());s.close()' ~/.claudy/ble.sock <<< '{"state":"done","message":"hi"}'`)

**GB2 passes if the screen updates** (Done / your message / 60%). Stop the daemon with Ctrl-C.

- [ ] **Step 4.3: Commit**

```bash
git add bridge/claudy-bled.py
git commit -m "bridge: claudy-bled.py — bleak daemon holding the BLE link + unix socket"
```

---

## Task 5: LaunchAgent install/uninstall

**Files:**
- Create: `bridge/install-ble-daemon.sh`
- Create: `bridge/uninstall-ble-daemon.sh`

**Goal:** One command sets up the venv + bleak + a LaunchAgent that runs the daemon at login and relaunches it on crash.

- [ ] **Step 5.1: Create `bridge/install-ble-daemon.sh`**

```bash
#!/bin/bash
# Install the Claudy BLE daemon as a macOS LaunchAgent.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
VENV="$HOME/.claudy/venv"
PLIST="$HOME/Library/LaunchAgents/com.claudy.bled.plist"
LABEL="com.claudy.bled"

echo "==> Creating venv at $VENV and installing bleak"
python3 -m venv "$VENV"
"$VENV/bin/pip" install --quiet --upgrade pip
"$VENV/bin/pip" install --quiet bleak

echo "==> Writing LaunchAgent $PLIST"
mkdir -p "$HOME/Library/LaunchAgents" "$HOME/.claudy"
cat > "$PLIST" <<PLISTEOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>Label</key><string>$LABEL</string>
  <key>ProgramArguments</key>
  <array>
    <string>$VENV/bin/python</string>
    <string>$HERE/claudy-bled.py</string>
  </array>
  <key>RunAtLoad</key><true/>
  <key>KeepAlive</key><true/>
  <key>StandardOutPath</key><string>$HOME/.claudy/bled.log</string>
  <key>StandardErrorPath</key><string>$HOME/.claudy/bled.log</string>
</dict>
</plist>
PLISTEOF

echo "==> Loading LaunchAgent"
launchctl unload "$PLIST" 2>/dev/null || true
launchctl load "$PLIST"

echo "==> Done. Daemon log: ~/.claudy/bled.log"
echo "    Set 'export CLAUDY_TRANSPORT=ble' in ~/.zshenv so hooks use BLE."
```

```bash
chmod +x bridge/install-ble-daemon.sh
```

- [ ] **Step 5.2: Create `bridge/uninstall-ble-daemon.sh`**

```bash
#!/bin/bash
set -euo pipefail
PLIST="$HOME/Library/LaunchAgents/com.claudy.bled.plist"
echo "==> Unloading + removing LaunchAgent"
launchctl unload "$PLIST" 2>/dev/null || true
rm -f "$PLIST"
echo "==> Done. (venv at ~/.claudy/venv left in place; rm -rf it to fully remove.)"
```

```bash
chmod +x bridge/uninstall-ble-daemon.sh
```

- [ ] **Step 5.3: Install and verify the daemon runs under launchd (USER)**

```bash
./bridge/install-ble-daemon.sh
sleep 3
launchctl list | grep com.claudy.bled    # should show the label with a PID
cat ~/.claudy/bled.log                    # should show "listening on..." + scanning/connected
```

Expected: a PID is listed and the log shows the daemon scanning/connecting. With the device powered, it should reach `connected`.

- [ ] **Step 5.4: Commit**

```bash
git add bridge/install-ble-daemon.sh bridge/uninstall-ble-daemon.sh
git commit -m "bridge: LaunchAgent install/uninstall for the BLE daemon"
```

---

## Task 6: Hook integration + end-to-end (GB3 + GB4)

**Files:** none new — configuration + verification.

**Goal:** Real Claude Code hooks drive the device over BLE in <200ms, and it survives reconnects / runs on battery.

- [ ] **Step 6.1: Point hooks at BLE**

Ensure the daemon is installed (Task 5) and add to `~/.zshenv` so every Claude-spawned shell inherits it:

```bash
echo 'export CLAUDY_TRANSPORT=ble' >> ~/.zshenv
```

If hooks aren't installed yet, run `./bridge/install-hooks.sh` (the existing installer; the hook calls `send-state.sh` → `send_state.py`, which now honors `CLAUDY_TRANSPORT`). Restart Claude Code so it re-reads settings + env.

- [ ] **Step 6.2: GB3 — end-to-end latency (USER)**

With the device powered (battery, untethered) and the daemon connected, use Claude Code: submit a prompt, run a Bash command, edit a file.

**GB3 passes if:** each tool call changes the screen within ~200ms (subjectively instant), the mascot animates per state, the message shows your prompt/file (Chinese included), and the token bar tracks.

If updates lag by seconds: check `~/.claudy/bled.log` — if it's reconnecting each time, the device may be power-sleeping its BLE; if `send_state.py` is slow, confirm `CLAUDY_TRANSPORT=ble` is actually set in the hook's environment (`launchctl getenv` won't show it; test by running a hook manually).

- [ ] **Step 6.3: GB4 — robustness (USER)**

- Kill the daemon (`launchctl kill TERM gui/$(id -u)/com.claudy.bled` or just `kill` the PID); KeepAlive should relaunch it within seconds (re-check `launchctl list`).
- Walk the device out of BLE range (or power it off) and back; the daemon log should show disconnect → scanning → reconnect, and the next hook event should land.
- Confirm the whole thing works with the device on **battery only** (USB unplugged from the Mac).

**GB4 passes** when reconnect is automatic and battery operation is confirmed.

- [ ] **Step 6.4: Commit** (only if any config files changed; otherwise skip)

No code change expected here — this task is integration + verification.

---

## Task 7: Documentation

**Files:**
- Create: `firmware-c6/README.md` (if not present) or modify it
- Modify: `README.md` (root) / `README.zh-TW.md`

**Goal:** Document the BLE option so it's reproducible.

- [ ] **Step 7.1: Add a BLE section to `firmware-c6/README.md`**

If `firmware-c6/README.md` doesn't exist yet, create it with this; otherwise append the section:

```markdown
## Bluetooth LE transport (no WiFi needed)

The ESP32-C6 is battery-powered and can run untethered over BLE instead of WiFi.

**Firmware:** in `firmware-c6/config.h`, define `CLAUDY_TRANSPORT_BLE` (comment
out `CLAUDY_TRANSPORT_WIFI`). Rebuild + flash. The device advertises as `claudy`.

**Mac side:**
```bash
./bridge/install-ble-daemon.sh        # venv + bleak + LaunchAgent (autostart)
echo 'export CLAUDY_TRANSPORT=ble' >> ~/.zshenv
./bridge/install-hooks.sh             # if not already installed
# restart Claude Code
```

The daemon (`claudy-bled.py`) holds the BLE link; hook events go
hook → `send_state.py` → `~/.claudy/ble.sock` → daemon → BLE → device.
Logs: `~/.claudy/bled.log`. Remove with `./bridge/uninstall-ble-daemon.sh`.

**Switching back to WiFi:** define `CLAUDY_TRANSPORT_WIFI` in config.h, rebuild,
and `unset CLAUDY_TRANSPORT` (or set it to `wifi`).
```

- [ ] **Step 7.2: One-line pointer in the root `README.md`**

Under the ESP32-C6 board bullet (added in the main port's Task 12, if present) or the Hardware section, add:

```
  - Battery / no-WiFi? The C6 also supports a **Bluetooth LE** transport — see [`firmware-c6/README.md`](firmware-c6/README.md).
```

- [ ] **Step 7.3: Commit**

```bash
git add firmware-c6/README.md README.md README.zh-TW.md
git commit -m "docs: BLE transport setup for the ESP32-C6 board"
```

---

## Verification Checklist (final)

- [ ] WiFi build still compiles (transport switch didn't break Task 9)
- [ ] BLE build fits flash (record %; custom partition only if it didn't)
- [ ] `pytest bridge/tests/test_send_ble.py` passes (both tests)
- [ ] GB1: nRF Connect write → screen updates
- [ ] GB2: daemon + socket line → screen updates
- [ ] GB3: real hooks drive the screen <200ms over BLE
- [ ] GB4: auto-reconnect + battery-only operation confirmed
- [ ] `~/.claudy/bled.log` shows clean connect/reconnect cycles
