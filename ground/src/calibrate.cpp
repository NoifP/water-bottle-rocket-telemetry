#include "calibrate.h"
#include "touch.h"
#include "config.h"
#include <TFT_eSPI.h>
#include <Arduino.h>

extern TFT_eSPI tft; // defined in display.cpp

// Crosshair target positions (screen pixels, inset from edges)
static const int16_t TGT_TL_X = 16,  TGT_TL_Y = 16;
static const int16_t TGT_BR_X = 224, TGT_BR_Y = 304;

static const uint16_t COL_BG    = TFT_BLACK;
static const uint16_t COL_TEXT  = TFT_WHITE;
static const uint16_t COL_DIM   = 0x4208; // dark grey
static const uint16_t COL_CROSS = TFT_WHITE;
static const uint16_t COL_HIT   = TFT_GREEN;

static void draw_crosshair(int16_t x, int16_t y, uint16_t color) {
    tft.drawFastHLine(x - 14, y, 29, color);
    tft.drawFastVLine(x, y - 14, 29, color);
    tft.fillCircle(x, y, 3, color);
}

static void draw_header(const char* title) {
    tft.fillScreen(COL_BG);
    tft.fillRect(0, 0, SCREEN_W, 22, 0x000F); // dark blue header
    tft.setTextColor(COL_TEXT, 0x000F);
    tft.setTextFont(2);
    tft.setCursor(4, 4);
    tft.print(title);
    tft.drawFastHLine(0, 22, SCREEN_W, COL_DIM);
}

// Collect an averaged raw touch sample. Waits up to timeout_ms for a touch,
// then averages samples over 300 ms, then waits for release.
// Returns false if no touch within timeout.
static bool sample_raw(int16_t& rx, int16_t& ry, uint32_t timeout_ms = 15000) {
    uint32_t deadline = millis() + timeout_ms;
    int16_t x, y;

    // Wait for touch
    while (millis() < deadline) {
        if (touch_get_raw(x, y)) break;
        delay(10);
    }
    if (millis() >= deadline) return false;

    // Average samples over 300 ms for accuracy
    delay(50); // let touch settle
    int32_t sum_x = 0, sum_y = 0;
    int count = 0;
    uint32_t end = millis() + 300;
    while (millis() < end) {
        if (touch_get_raw(x, y)) {
            sum_x += x;
            sum_y += y;
            count++;
        }
        delay(15);
    }
    rx = (count > 0) ? (sum_x / count) : x;
    ry = (count > 0) ? (sum_y / count) : y;

    // Wait for release
    while (touch_get_raw(x, y)) delay(10);
    delay(200); // debounce gap

    return true;
}

void calibrate_run() {
    // --- Gate: hold for 2 seconds to proceed, release to skip ---
    draw_header("TOUCH CALIBRATION");
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.setTextFont(2);
    tft.setCursor(20, 140);
    tft.print("Hold to calibrate...");
    tft.setCursor(32, 160);
    tft.print("Release to skip");

    uint32_t hold_start = millis();
    int16_t _x, _y;
    while (millis() - hold_start < 2000) {
        if (!touch_get_raw(_x, _y)) return; // released early — skip
        int bar_w = (int)((millis() - hold_start) * SCREEN_W / 2000);
        tft.fillRect(0, 195, bar_w, 6, TFT_GREEN);
        tft.fillRect(bar_w, 195, SCREEN_W - bar_w, 6, COL_DIM);
        delay(30);
    }
    // Wait for release before showing first target
    while (touch_get_raw(_x, _y)) delay(10);
    delay(250);

    // --- Step 1: top-left ---
    draw_header("TOUCH CALIBRATION");
    tft.setTextColor(TFT_CYAN, COL_BG);
    tft.setTextFont(2);
    tft.setCursor(8, 30);
    tft.print("Tap the crosshair  (1 of 2)");
    draw_crosshair(TGT_TL_X, TGT_TL_Y, COL_CROSS);

    int16_t rx1, ry1;
    if (!sample_raw(rx1, ry1)) return; // timeout — abort
    draw_crosshair(TGT_TL_X, TGT_TL_Y, COL_HIT);
    delay(350);

    // --- Step 2: bottom-right ---
    draw_header("TOUCH CALIBRATION");
    tft.setTextColor(TFT_CYAN, COL_BG);
    tft.setTextFont(2);
    tft.setCursor(8, 30);
    tft.print("Tap the crosshair  (2 of 2)");
    draw_crosshair(TGT_BR_X, TGT_BR_Y, COL_CROSS);

    int16_t rx2, ry2;
    if (!sample_raw(rx2, ry2)) return; // timeout — abort
    draw_crosshair(TGT_BR_X, TGT_BR_Y, COL_HIT);
    delay(350);

    // --- Compute calibration ---
    // Min/max cover the raw range regardless of axis direction.
    // TOUCH_INVERT_X/Y (from config.h) handle direction — we only calibrate range.
    int16_t min_x = min(rx1, rx2);
    int16_t max_x = max(rx1, rx2);
    int16_t min_y = min(ry1, ry2);
    int16_t max_y = max(ry1, ry2);

    // Sanity check: reject degenerate results (both taps on same spot)
    if (max_x - min_x < 100 || max_y - min_y < 100) {
        draw_header("TOUCH CALIBRATION");
        tft.setTextColor(TFT_RED, COL_BG);
        tft.setTextFont(2);
        tft.setCursor(20, 150);
        tft.print("Bad readings — try again");
        delay(3000);
        return;
    }

    touch_set_calibration(min_x, max_x, min_y, max_y);
    touch_save_calibration();

    // --- Result screen ---
    draw_header("TOUCH CALIBRATION");
    tft.setTextColor(TFT_GREEN, COL_BG);
    tft.setTextFont(2);
    tft.setCursor(40, 130);
    tft.print("Calibration saved!");

    char buf[32];
    tft.setTextColor(COL_DIM, COL_BG);
    snprintf(buf, sizeof(buf), "X: %d - %d", min_x, max_x);
    tft.setCursor(60, 158);
    tft.print(buf);
    snprintf(buf, sizeof(buf), "Y: %d - %d", min_y, max_y);
    tft.setCursor(60, 174);
    tft.print(buf);

    tft.setTextColor(COL_TEXT, COL_BG);
    tft.setCursor(30, 205);
    tft.print("Resuming in 3 seconds...");
    delay(3000);
}
