#pragma once
#include <stddef.h>

// Initialize the soft AP and HTTP server. Must be called before comms_init()
// so that WiFi is set to WIFI_AP_STA before ESP-NOW is registered.
void timesync_init();

// Stop the soft AP (keeps WiFi in AP_STA mode for ESP-NOW compatibility).
void timesync_stop();

// Re-enable the soft AP after a previous stop (e.g. user wants to resync).
void timesync_restart();

// Handle pending HTTP requests. Call every loop iteration.
void timesync_update();

bool timesync_is_running();
bool timesync_is_synced();

// Fills buf with:
//   "2026-04-14 10:23:45"          — when time has been set
//   "SSID: WaterRocketGS  10.0.0.42" — otherwise
void timesync_status_line(char* buf, size_t len);
