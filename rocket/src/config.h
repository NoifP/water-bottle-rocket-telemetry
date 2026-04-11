#pragma once
#include <Arduino.h>

// ============================================================================
// Rocket configuration -- pins, timing, thresholds
// ============================================================================

// --- Pin assignments (Wemos S2 Mini) ---
#define PIN_SDA          33
#define PIN_SCL          35
#define PIN_SERVO        11
#define PIN_BATTERY_ADC  3
#define PIN_LED          15

// --- Timing ---
#define LOOP_INTERVAL_US      20000   // 50 Hz = 20ms
#define IDLE_LOOP_INTERVAL_US 500000  // 2 Hz in idle power-save mode
#define IDLE_TIMEOUT_MS       10000   // Drop to low-power after 10s with no commands

// --- Apogee detection ---
#define APOGEE_ALT_DROP_M      0.5f   // Altitude must drop this much from peak
#define APOGEE_CONFIRM_MS      100    // Sustained for this long
#define FREEFALL_THRESHOLD_MS2 2.0f   // Accel magnitude below this = freefall
#define FREEFALL_CONFIRM_MS    80

// --- State transitions ---
#define BOOST_ACCEL_THRESHOLD  30.0f  // m/s^2 (~3g)
#define BOOST_CONFIRM_SAMPLES  3      // Consecutive samples above threshold
#define COAST_ACCEL_THRESHOLD  15.0f  // m/s^2 -- thrust ended
#define LANDED_SPEED_THRESHOLD 0.3f   // m/s
#define LANDED_ALT_THRESHOLD   2.0f   // metres AGL
#define LANDED_CONFIRM_MS      3000
#define ARMED_TIMEOUT_MS       900000 // 15 minutes

// --- Servo ---
#define SERVO_LOCKED_ANGLE     0      // Degrees -- holds parachute
#define SERVO_DEPLOY_ANGLE     90     // Degrees -- releases parachute
#define SERVO_DETACH_DELAY_MS  1000   // Detach servo after deploy to save power

// --- Battery ---
#define BATTERY_DIVIDER_RATIO  2.0f   // Voltage divider ratio
#define BATTERY_LOW_V          3.3f   // Warn below this voltage

// --- Altitude filter ---
#define VSPEED_ALPHA           0.3f   // Exponential smoothing factor for vertical speed

// --- ESP-NOW ---
#define ESPNOW_CHANNEL         1
// Ground station MAC address -- update this to match your ground station
#if !__has_include("config_local.h")
static const uint8_t GROUND_MAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // broadcast
#endif

// Local overrides (gitignored) -- create config_local.h to override any of the above
#if __has_include("config_local.h")
#include "config_local.h"
#endif
