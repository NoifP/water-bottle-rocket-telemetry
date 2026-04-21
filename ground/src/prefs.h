#pragma once
#include <stdint.h>

enum SpeedUnit { SPEED_MS = 0, SPEED_KMH = 1 };

void      prefs_init();
SpeedUnit prefs_get_speed_unit();
void      prefs_set_speed_unit(SpeedUnit u);
bool      prefs_get_gravity_offset();   // true = subtract 1g from accel display
void      prefs_set_gravity_offset(bool enabled);

// Timezone offset in minutes west of UTC (JS getTimezoneOffset() convention).
// Positive = west (e.g. UTC-5 → +300), negative = east (e.g. UTC+10 → -600).
int16_t   prefs_get_tz_offset();
void      prefs_set_tz_offset(int16_t minutes); // saves to NVS and applies immediately

// ESP-NOW WiFi channel (1..13). Persisted in the "radio" NVS namespace.
uint8_t   prefs_get_channel();
void      prefs_set_channel(uint8_t ch); // caller is responsible for re-configuring the radio
