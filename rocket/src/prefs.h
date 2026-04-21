#pragma once
#include <stdint.h>

// Loads persisted settings from NVS. Call once, early in setup() — must run
// before comms_init() so the saved channel is available.
void    prefs_init();

// Last known ESP-NOW WiFi channel (1..13). Defaults to ESPNOW_CHANNEL_DEFAULT
// if no value has been saved yet.
uint8_t prefs_get_channel();

// Persist a new channel to NVS.
void    prefs_set_channel(uint8_t ch);
