#pragma once

// ============================================================================
// Ground Station configuration -- pins, timing, calibration
// ============================================================================

// --- Touch pins (XPT2046, software SPI) ---
#define TOUCH_CS   33
#define TOUCH_IRQ  36
#define TOUCH_CLK  25
#define TOUCH_MOSI 32
#define TOUCH_MISO 39

// --- Touch calibration (raw ADC 0-4095 -> screen 0-239/0-319) ---
#define TOUCH_MIN_X    200
#define TOUCH_MAX_X    3800
#define TOUCH_MIN_Y    200
#define TOUCH_MAX_Y    3800
#define TOUCH_SWAP_XY  false
#define TOUCH_INVERT_X false
#define TOUCH_INVERT_Y true

// --- Touch timing ---
#define TOUCH_DEBOUNCE_MS    50
#define TOUCH_COOLDOWN_MS    300

// --- SD card ---
#define SD_CS 5  // VSPI: SCK=18, MISO=19, MOSI=23

// --- RGB LED (active LOW) ---
#define LED_R 4
#define LED_G 16
#define LED_B 17

// --- Display update rates ---
#define DISPLAY_UPDATE_MS    100   // 10 Hz
#define TOUCH_POLL_MS        50    // 20 Hz
#define SIGNAL_CALC_MS       1000  // 1 Hz
#define SD_FLUSH_INTERVAL_MS 2000  // Flush every 2 seconds
#define SIGNAL_LOST_MS       2000  // No packet for 2s = signal lost

// --- SD ring buffer ---
#define SD_RING_BUFFER_SIZE 128

// --- ESP-NOW ---
#define ESPNOW_CHANNEL 1
// Rocket MAC address -- update this to match your rocket
static const uint8_t ROCKET_MAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // broadcast

// --- Screen dimensions ---
#define SCREEN_W 240
#define SCREEN_H 320
