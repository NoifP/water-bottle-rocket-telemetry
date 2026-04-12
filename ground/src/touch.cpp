#include "touch.h"
#include "config.h"
#include <XPT2046_Touchscreen.h>
#include <Preferences.h>
#include <SPI.h>

// Touch uses separate SPI pins from the TFT
static SPIClass touchSPI(VSPI);
static XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

static bool deploy_enabled = false;
static bool confirm_mode = false;
static uint32_t last_press_ms = 0;

// Runtime calibration values — initialised from config.h, overridable from NVS
static int16_t cal_min_x = TOUCH_MIN_X;
static int16_t cal_max_x = TOUCH_MAX_X;
static int16_t cal_min_y = TOUCH_MIN_Y;
static int16_t cal_max_y = TOUCH_MAX_Y;

// Button regions (screen coordinates)
struct ButtonRect {
    int16_t x, y, w, h;
};

// Main screen buttons
static const ButtonRect btn_arm_rect    = {8, 230, 108, 44};
static const ButtonRect btn_deploy_rect = {124, 230, 108, 44};

// Confirmation dialog buttons
static const ButtonRect btn_yes_rect = {40, 175, 70, 36};
static const ButtonRect btn_no_rect  = {130, 175, 70, 36};

static bool rect_contains(const ButtonRect& r, int16_t tx, int16_t ty) {
    return tx >= r.x && tx < r.x + r.w && ty >= r.y && ty < r.y + r.h;
}

static void map_touch(int16_t raw_x, int16_t raw_y, int16_t& sx, int16_t& sy) {
    int16_t mx = raw_x;
    int16_t my = raw_y;

    if (TOUCH_SWAP_XY) {
        int16_t tmp = mx;
        mx = my;
        my = tmp;
    }

    sx = map(mx, cal_min_x, cal_max_x, 0, SCREEN_W - 1);
    sy = map(my, cal_min_y, cal_max_y, 0, SCREEN_H - 1);

    if (TOUCH_INVERT_X) sx = SCREEN_W - 1 - sx;
    if (TOUCH_INVERT_Y) sy = SCREEN_H - 1 - sy;

    // Clamp
    if (sx < 0) sx = 0;
    if (sx >= SCREEN_W) sx = SCREEN_W - 1;
    if (sy < 0) sy = 0;
    if (sy >= SCREEN_H) sy = SCREEN_H - 1;
}

void touch_init() {
    touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
    ts.begin(touchSPI);
    ts.setRotation(0); // Portrait

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

    Serial.println("[touch] Initialized");
}

ButtonId touch_update() {
    uint32_t now = millis();

    if (!ts.touched()) return BTN_NONE;

    // Debounce / cooldown
    if (now - last_press_ms < TOUCH_COOLDOWN_MS) return BTN_NONE;

    TS_Point p = ts.getPoint();
    int16_t sx, sy;
    map_touch(p.x, p.y, sx, sy);

    last_press_ms = now;

    if (confirm_mode) {
        if (rect_contains(btn_yes_rect, sx, sy)) return BTN_CONFIRM_YES;
        if (rect_contains(btn_no_rect, sx, sy))  return BTN_CONFIRM_NO;
        return BTN_NONE;
    }

    if (rect_contains(btn_arm_rect, sx, sy)) return BTN_ARM;
    if (deploy_enabled && rect_contains(btn_deploy_rect, sx, sy)) return BTN_DEPLOY;

    return BTN_NONE;
}

void touch_set_deploy_enabled(bool enabled) {
    deploy_enabled = enabled;
}

void touch_set_confirm_mode(bool active) {
    confirm_mode = active;
}

bool touch_get_raw(int16_t& x, int16_t& y) {
    if (!ts.touched()) return false;
    TS_Point p = ts.getPoint();
    x = p.x;
    y = p.y;
    return true;
}

void touch_set_calibration(int16_t min_x, int16_t max_x, int16_t min_y, int16_t max_y) {
    cal_min_x = min_x;
    cal_max_x = max_x;
    cal_min_y = min_y;
    cal_max_y = max_y;
}

void touch_save_calibration() {
    Preferences prefs;
    prefs.begin("touch_cal", false); // read-write
    prefs.putBool("valid", true);
    prefs.putShort("min_x", cal_min_x);
    prefs.putShort("max_x", cal_max_x);
    prefs.putShort("min_y", cal_min_y);
    prefs.putShort("max_y", cal_max_y);
    prefs.end();
    Serial.printf("[touch] Calibration saved: X %d-%d  Y %d-%d\n",
                  cal_min_x, cal_max_x, cal_min_y, cal_max_y);
}
