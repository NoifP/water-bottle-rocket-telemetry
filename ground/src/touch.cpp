#include "touch.h"
#include "config.h"
#include <Preferences.h>
#include <Arduino.h>

// Runtime calibration values — initialised from config.h, overridable from NVS
static int16_t cal_min_x = TOUCH_MIN_X;
static int16_t cal_max_x = TOUCH_MAX_X;
static int16_t cal_min_y = TOUCH_MIN_Y;
static int16_t cal_max_y = TOUCH_MAX_Y;

static int16_t last_sx = -1;
static int16_t last_sy = -1;
static int16_t last_rx = -1;
static int16_t last_ry = -1;

static bool deploy_enabled = false;
static bool confirm_mode   = false;
static uint32_t last_press_ms = 0;

// Button regions (screen coordinates)
struct ButtonRect { int16_t x, y, w, h; };
static const ButtonRect btn_arm_rect    = {8,   230, 108, 44};
static const ButtonRect btn_deploy_rect = {124, 230, 108, 44};
static const ButtonRect btn_yes_rect    = {40,  175, 70,  36};
static const ButtonRect btn_no_rect     = {130, 175, 70,  36};

static bool rect_contains(const ButtonRect& r, int16_t tx, int16_t ty) {
    return tx >= r.x && tx < r.x + r.w && ty >= r.y && ty < r.y + r.h;
}

// ---- Bit-bang SPI for XPT2046 (mode 0, MSB first) -------------------------
//
// The XPT2046 touch controller is wired to GPIO pins that conflict with the
// SD card's VSPI bus, so we drive it with direct GPIO bit-banging instead of
// hardware SPI. The chip runs at up to 2.5 MHz; bit-bang at ~500 kHz is fine.

static uint16_t xpt_transfer(uint8_t cmd) {
    // Send 8-bit command, MSB first; data is clocked on the rising edge
    for (int i = 7; i >= 0; i--) {
        digitalWrite(TOUCH_MOSI, (cmd >> i) & 1);
        digitalWrite(TOUCH_CLK, HIGH);
        delayMicroseconds(1);
        digitalWrite(TOUCH_CLK, LOW);
        delayMicroseconds(1);
    }
    // Read 16-bit response; XPT2046 places the 12-bit result in bits [14:3]
    uint16_t result = 0;
    for (int i = 15; i >= 0; i--) {
        digitalWrite(TOUCH_CLK, HIGH);
        delayMicroseconds(1);
        if (digitalRead(TOUCH_MISO)) result |= (1u << i);
        digitalWrite(TOUCH_CLK, LOW);
        delayMicroseconds(1);
    }
    return result >> 3; // 12-bit result (0-4095)
}

// Non-pipelined single-channel read: assert CS, send cmd, read result, deassert CS.
// Each call is a self-contained transaction so there is no pipeline offset to manage.
// The final transaction uses PD=00 (cmd 0x90) to re-enable PENIRQ; all others use
// PD=01 (PENIRQ disabled during burst to keep the line stable).
static uint16_t xpt_read_one(uint8_t cmd) {
    digitalWrite(TOUCH_CS, LOW);
    delayMicroseconds(5);
    uint16_t val = xpt_transfer(cmd);
    digitalWrite(TOUCH_CS, HIGH);
    delayMicroseconds(5);
    return val;
}

// Read raw ADC values from the XPT2046. Returns false if screen not touched.
static bool xpt_read(int16_t& raw_x, int16_t& raw_y) {
    if (digitalRead(TOUCH_IRQ) == HIGH) return false;

    // Discard first X and Y readings (ADC input settling after IRQ fires)
    xpt_read_one(0xD1); // X, PD=01
    xpt_read_one(0x91); // Y, PD=01

    // Average 4 samples for noise rejection
    int32_t sx = 0, sy = 0;
    for (int i = 0; i < 4; i++) {
        sx += xpt_read_one(0xD1); // X, PD=01
        sy += xpt_read_one(0x91); // Y, PD=01
    }

    // Final transaction with PD=00: re-enables PENIRQ so the next touch is detected
    xpt_read_one(0x90); // Y, PD=00 — result discarded

    raw_x = (int16_t)(sx / 4);
    raw_y = (int16_t)(sy / 4);
    return true;
}

// ---------------------------------------------------------------------------

static void map_touch(int16_t raw_x, int16_t raw_y, int16_t& sx, int16_t& sy) {
    int16_t mx = raw_x;
    int16_t my = raw_y;

    if (TOUCH_SWAP_XY) {
        int16_t tmp = mx; mx = my; my = tmp;
    }

    sx = map(mx, cal_min_x, cal_max_x, 0, SCREEN_W - 1);
    sy = map(my, cal_min_y, cal_max_y, 0, SCREEN_H - 1);

    if (TOUCH_INVERT_X) sx = SCREEN_W - 1 - sx;
    if (TOUCH_INVERT_Y) sy = SCREEN_H - 1 - sy;

    if (sx < 0)          sx = 0;
    if (sx >= SCREEN_W)  sx = SCREEN_W - 1;
    if (sy < 0)          sy = 0;
    if (sy >= SCREEN_H)  sy = SCREEN_H - 1;
}

void touch_init() {
    pinMode(TOUCH_CS,   OUTPUT); digitalWrite(TOUCH_CS,   HIGH);
    pinMode(TOUCH_CLK,  OUTPUT); digitalWrite(TOUCH_CLK,  LOW);
    pinMode(TOUCH_MOSI, OUTPUT); digitalWrite(TOUCH_MOSI, LOW);
    pinMode(TOUCH_MISO, INPUT);
    pinMode(TOUCH_IRQ,  INPUT);

    // Load calibration from NVS if a saved calibration exists
    Preferences prefs;
    prefs.begin("touch_cal", true); // read-only
    if (prefs.getBool("valid", false)) {
        cal_min_x = prefs.getShort("min_x", TOUCH_MIN_X);
        cal_max_x = prefs.getShort("max_x", TOUCH_MAX_X);
        cal_min_y = prefs.getShort("min_y", TOUCH_MIN_Y);
        cal_max_y = prefs.getShort("max_y", TOUCH_MAX_Y);
        Serial.printf("[touch] Calibration from NVS: X %d-%d  Y %d-%d\n",
                      cal_min_x, cal_max_x, cal_min_y, cal_max_y);
    } else {
        Serial.println("[touch] No saved calibration, using config.h defaults");
    }
    prefs.end();

    Serial.println("[touch] Initialized (bit-bang SPI)");
}

ButtonId touch_update() {
    uint32_t now = millis();

    int16_t rx, ry;
    if (!xpt_read(rx, ry)) return BTN_NONE;

    // Map and store coordinates on every touch for display feedback
    int16_t sx, sy;
    map_touch(rx, ry, sx, sy);
    last_rx = rx;
    last_ry = ry;
    last_sx = sx;
    last_sy = sy;

    // Cooldown gates button events only — not coordinate display
    if (now - last_press_ms < TOUCH_COOLDOWN_MS) return BTN_NONE;

    last_press_ms = now;

    if (confirm_mode) {
        if (rect_contains(btn_yes_rect, sx, sy)) return BTN_CONFIRM_YES;
        if (rect_contains(btn_no_rect,  sx, sy)) return BTN_CONFIRM_NO;
        return BTN_NONE;
    }

    if (rect_contains(btn_arm_rect, sx, sy)) return BTN_ARM;
    if (deploy_enabled && rect_contains(btn_deploy_rect, sx, sy)) return BTN_DEPLOY;

    return BTN_NONE;
}

void touch_set_deploy_enabled(bool enabled) { deploy_enabled = enabled; }
void touch_set_confirm_mode(bool active)    { confirm_mode = active; }

bool touch_get_raw(int16_t& x, int16_t& y) {
    return xpt_read(x, y);
}

void touch_get_last_pos(int16_t& x, int16_t& y) {
    x = last_sx;
    y = last_sy;
}

void touch_get_last_raw(int16_t& x, int16_t& y) {
    x = last_rx;
    y = last_ry;
}

void touch_set_calibration(int16_t min_x, int16_t max_x, int16_t min_y, int16_t max_y) {
    cal_min_x = min_x;
    cal_max_x = max_x;
    cal_min_y = min_y;
    cal_max_y = max_y;
}

void touch_save_calibration() {
    Preferences prefs;
    prefs.begin("touch_cal", false);
    prefs.putBool("valid", true);
    prefs.putShort("min_x", cal_min_x);
    prefs.putShort("max_x", cal_max_x);
    prefs.putShort("min_y", cal_min_y);
    prefs.putShort("max_y", cal_max_y);
    prefs.end();
    Serial.printf("[touch] Calibration saved: X %d-%d  Y %d-%d\n",
                  cal_min_x, cal_max_x, cal_min_y, cal_max_y);
}
