# Claudy BLE Transport — Design

**Date:** 2026-06-01
**Status:** Draft, awaiting user review
**Applies to:** `firmware-c6/` (Waveshare ESP32-C6 Touch AMOLED 2.16) + `bridge/`
**Context:** The ESP32-C6 device is battery-powered and the user has no usable
2.4GHz WiFi (company 2.4G disabled, phone hotspot won't connect). WiFi (Task 9)
works but can't be used in this environment. The ESP32-C6 has Bluetooth LE
(no Classic / no SPP), so we add a BLE transport as an alternative to WiFi.

## 1. Goals & Non-Goals

### Goals
- Drive the device's `g_state` from Claude Code hooks over **Bluetooth LE**, so
  the battery-powered device works with no WiFi and no USB tether.
- Keep reaction latency comparable to WiFi (<200ms) during normal use.
- Preserve the existing WiFi path (Task 9) intact — selectable, not deleted.
- Reuse the existing AppState / JSON model and the LVGL UI unchanged.
- Never block Claude Code (fire-and-forget semantics, like the HTTP bridge).

### Non-Goals (v1)
- No BLE pairing/bonding/encryption (open GATT). Optional token deferred.
- No replay queue — only the latest state matters; stale events are dropped.
- No simultaneous WiFi+BLE in one firmware build (flash budget; see §3).
- No change to the mascot, layout, fonts, or touch behavior.

## 2. Architecture

```
Claude Code hook (Mac)
  └─ bridge/send-state.sh → bridge/send_state.py   [CLAUDY_TRANSPORT=ble]
       └─ write one state-JSON line to ~/.claudy/ble.sock   (local, non-blocking)
            └─ bridge/claudy-bled.py   (background daemon; holds the BLE link)
                 └─ BLE GATT write to the "state" characteristic
                      └─ ESP32-C6 ble.cpp onWrite()
                           └─ parse JSON → g_state → ui_apply_state()  (under lvgl_port_lock)
```

Only the transport layer changes. The mascot, LVGL port, UI widgets, fonts,
touch, PMIC, and CO5300 display code are untouched.

### Compile-time transport switch

We are at ~91% of the 3MB app partition with WiFi compiled in. Compiling WiFi
**and** BLE together would overflow. So `firmware-c6/config.h` gains one selector:

```c
// Exactly one of these:
#define CLAUDY_TRANSPORT_BLE
// #define CLAUDY_TRANSPORT_WIFI
```

- `CLAUDY_TRANSPORT_BLE`: compile `src/net/ble.cpp`; `src/net/net.cpp` body is
  wrapped in `#ifdef CLAUDY_TRANSPORT_WIFI` so the WiFi/lwIP/WebServer stack is
  not linked → its flash is freed for the NimBLE stack.
- `CLAUDY_TRANSPORT_WIFI`: the reverse — original Task 9 behavior, unchanged.
- `firmware-c6.ino` chooses `bleBegin()` vs `netBegin()` / `netLoop()` via the
  same `#ifdef`.

This trades the WiFi stack for the BLE stack rather than stacking both, so net
flash should stay close to the current 91%. Verified empirically before
proceeding (see §3 flash gate).

## 3. Firmware (ESP32-C6)

### New module: `src/net/ble.{h,cpp}`

Library: **NimBLE-Arduino** (h2zero) — far lighter on flash/RAM than the default
Bluedroid `BLEDevice` stack, and supports the ESP32-C6. Added to `sketch.yaml`.

`ble.h`:
```c
#pragma once
#include <Arduino.h>
bool bleBegin();        // start GATT server + advertising; returns true on success
bool bleConnected();    // is a central currently connected?
```

`ble.cpp` responsibilities:
- Init NimBLE, set device name `MDNS_HOSTNAME` ("claudy" → advertises as such; we
  keep the name in config so it matches the WiFi hostname).
- One GATT **service** (custom 128-bit UUID) with one **characteristic** "state":
  properties WRITE + WRITE_NR, max 512 bytes (BLE attribute limit; our state JSON
  is < 256 bytes).
- `onWrite` callback: copy the received bytes into a buffer, then reuse the SAME
  parse path as the HTTP handler — `parseStateName`, `parseToolName`, copy
  message, tokens — mutate `g_state` and call `ui_apply_state()` inside
  `lvgl_port_lock(200)/unlock`.
- Advertise on start and re-advertise on disconnect (so the daemon can
  reconnect). NimBLE runs its own task; `loop()` does not need a `bleLoop()`.

### Service / characteristic UUIDs
Fixed custom 128-bit UUIDs, identical in firmware and daemon:
- Service:     `c1a0dc00-1f9b-4e3a-9b00-a1b2c3d4e5f6`
- State char:  `c1a0dc01-1f9b-4e3a-9b00-a1b2c3d4e5f6`

### `firmware-c6.ino` changes
- Include `src/net/ble.h` (under `#ifdef CLAUDY_TRANSPORT_BLE`).
- In `setup()`, after `ui_init()`: `#ifdef CLAUDY_TRANSPORT_BLE` → `bleBegin()`,
  set state IDLE with message "Bluetooth: ready" (or device name); `#else`
  → existing `netBegin()` block.
- In `loop()`: the `netLoop()` call is `#ifdef CLAUDY_TRANSPORT_WIFI` only.
  Idle-timeout + tap-to-wake + `lvgl_port_tick()` stay for both.

### Flash gate (must pass before building the bridge)
Build the BLE variant and confirm it fits the 3MB partition (< 100%, ideally
< 95%). If it overflows: contingency is a **custom partition** (`partitions.csv`
+ `PartitionScheme=custom`) reclaiming the unused 9.9MB FAT region for the app,
which removes the flash ceiling entirely. Only done if measured to be necessary.

## 4. Bridge (Mac)

### New daemon: `bridge/claudy-bled.py`
Built on **`bleak`** (asyncio BLE). A single asyncio process running two tasks:

1. **BLE link task:** scan for the device (by name `claudy` + service UUID),
   connect, hold the connection. On disconnect, re-scan with capped backoff
   (e.g. 1s → 2s → 5s → 5s…) and reconnect.
2. **Socket server task:** `asyncio.start_unix_server` on `~/.claudy/ble.sock`.
   Each client connection sends one JSON line; the daemon writes it to the state
   characteristic via the held BLE client (`write_gatt_char`, response=False).

State handling: keep only the **latest** state in memory. If a write arrives
while disconnected, store it and push on reconnect. No queue/replay.

Logging: a rotating log at `~/.claudy/bled.log` for debugging connection issues.

### `bridge/send_state.py` change
Add a transport branch keyed on `CLAUDY_TRANSPORT` (env, default `wifi`):
- `wifi` (default): existing HTTP POST — unchanged.
- `ble`: connect to `~/.claudy/ble.sock`, send the JSON line, close. On any
  error (socket missing = daemon down, etc.) **fail silently** within the same
  short timeout budget — never block Claude. The state→JSON mapping is shared
  with the WiFi path (single source of truth).

### Daemon lifecycle (macOS)
- `bridge/install-ble-daemon.sh`: create `~/.claudy/venv`, `pip install bleak`,
  drop a LaunchAgent plist (`~/Library/LaunchAgents/com.claudy.bled.plist`) that
  runs the daemon at login with `KeepAlive` (relaunch on crash), and `launchctl
  load` it.
- `bridge/uninstall-ble-daemon.sh`: `launchctl unload` + remove the plist.

### Dependencies
The existing bridge is dependency-free (urllib). The daemon needs `bleak`,
isolated in `~/.claudy/venv` so the system Python is untouched. `send_state.py`
stays dependency-free (it only writes to a unix socket in BLE mode).

## 5. Error handling & robustness

| Situation | Behavior |
|---|---|
| Normal | hook → socket → daemon → BLE → screen, <200ms |
| Daemon not running | `send_state.py` silent no-op; Claude never blocked |
| Device off / out of range | daemon retries scan+connect with backoff; hooks no-op |
| Device returns | daemon reconnects, pushes latest state |
| Malformed JSON over BLE | firmware ignores (same as HTTP handler) |
| Daemon crash | LaunchAgent KeepAlive relaunches it |

## 6. Security (v1)
Open GATT, no bonding. The only exposure is a nearby BLE central writing a state
to the mascot (cosmetic, harmless; malformed input ignored). If locking down is
ever wanted, reuse `AUTH_TOKEN`: require it as a field in the JSON and have the
firmware drop writes that don't match. Deferred — not in v1.

## 7. Testing gates

| Gate | What | How |
|---|---|---|
| **Flash** | BLE build fits 3MB app partition | `build-c6.sh`, read flash % |
| **GB1** | Firmware advertises as `Claudy`; a BLE scanner connects + writes the state characteristic and the screen updates | nRF Connect (phone) or a `bleak` scan/write script |
| **GB2** | Daemon connects, holds, forwards socket → BLE | run daemon, pipe a JSON line to `~/.claudy/ble.sock`, watch screen |
| **GB3** | End-to-end: real Claude Code hooks drive the screen <200ms | install hooks with `CLAUDY_TRANSPORT=ble`, use Claude |
| **GB4** | Robustness: kill/restart daemon, walk device out of range & back, run on battery un-tethered | manual |

## 8. File structure

```
firmware-c6/
  config.h                 # + CLAUDY_TRANSPORT_BLE / _WIFI selector
  sketch.yaml              # + NimBLE-Arduino library
  src/net/
    net.{h,cpp}            # WiFi (Task 9) — body guarded by #ifdef CLAUDY_TRANSPORT_WIFI
    ble.{h,cpp}            # NEW — NimBLE GATT server, #ifdef CLAUDY_TRANSPORT_BLE
  firmware-c6.ino          # #ifdef-selects bleBegin() vs netBegin()/netLoop()

bridge/
  send_state.py            # + CLAUDY_TRANSPORT=ble branch (writes to unix socket)
  claudy-bled.py           # NEW — bleak daemon: BLE link + unix socket server
  install-ble-daemon.sh    # NEW — venv + bleak + LaunchAgent
  uninstall-ble-daemon.sh  # NEW
```

## 9. Open questions (deferred past v1)
- Battery telemetry on screen (AXP2101 already reads ~3950mV) — unrelated, defer.
- BLE token auth (§6) — defer unless wanted.
- A single firmware that runtime-switches WiFi/BLE — rejected for v1 (flash);
  compile-time switch is the chosen approach.

## 10. References
- WiFi/HTTP design + plan: `docs/superpowers/specs/2026-05-28-esp32-c6-amoled-port-design.md`,
  `docs/superpowers/plans/2026-05-28-esp32-c6-amoled-port.md`
- Existing bridge: `bridge/send_state.py`, `bridge/send-state.sh`
- NimBLE-Arduino: https://github.com/h2zero/NimBLE-Arduino
- bleak: https://github.com/hbldh/bleak
