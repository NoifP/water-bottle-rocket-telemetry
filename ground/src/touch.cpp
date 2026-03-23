#include "touch.h"
#include "config.h"
#include <XPT2046_Touchscreen.h>
#include <SPI.h>

// Touch uses separate SPI pins from the TFT
static SPIClass touchSPI(VSPI);
static XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

static bool deploy_enabled = false;
static bool confirm_mode = false;
static uint32_t last_press_ms = 0;

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

    sx = map(mx, TOUCH_MIN_X, TOUCH_MAX_X, 0, SCREEN_W - 1);
    sy = map(my, TOUCH_MIN_Y, TOUCH_MAX_Y, 0, SCREEN_H - 1);

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
