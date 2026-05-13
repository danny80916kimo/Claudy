#pragma once
#include <Arduino.h>

// Connects to WiFi (blocking with timeout), starts mDNS, and starts the HTTP server.
// Returns true if WiFi connected.
bool netBegin();

// Call from loop() to service WebServer + mDNS.
void netLoop();

// True if connected to WiFi.
bool netIsConnected();
