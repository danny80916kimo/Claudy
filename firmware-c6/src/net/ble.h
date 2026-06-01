#pragma once
#include <Arduino.h>

// Start the NimBLE GATT server + advertising. Returns true on success.
bool bleBegin();

// Is a central (the Mac daemon) currently connected?
bool bleConnected();
