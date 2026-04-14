#pragma once

#include <stdint.h>

// --- Hit-test helpers (shared with screen modules and main.cpp) ---
struct ButtonRect { int16_t x, y, w, h; };
bool rect_contains(const ButtonRect& r, int16_t tx, int16_t ty);

// --- Button IDs ---
enum ButtonId {
    BTN_NONE = 0,
    BTN_ARM,
    BTN_DEPLOY,
    BTN_CONFIRM_YES,
    BTN_CONFIRM_NO,
    BTN_MENU = 10,   // ≡ icon tapped in header (sx >= 210, sy < 24)
};

// Initialize touchscreen GPIO and load NVS calibration
void touch_init();

// Poll touch and return button ID. Call at 20 Hz.
// Also sets tap_pending for touch_get_tap() below.
ButtonId touch_update();

// Set whether the deploy button is enabled (greyed out if disabled)
void touch_set_deploy_enabled(bool enabled);

// Set whether we're showing the confirmation dialog
void touch_set_confirm_mode(bool active);

// Get raw touch coordinates (for calibration wizard)
bool touch_get_raw(int16_t& x, int16_t& y);

// Last mapped screen coordinates from any touch (x/y = -1 if never touched)
void touch_get_last_pos(int16_t& x, int16_t& y);

// Last raw ADC coordinates from any touch
void touch_get_last_raw(int16_t& x, int16_t& y);

// Override calibration min/max values at runtime
void touch_set_calibration(int16_t min_x, int16_t max_x, int16_t min_y, int16_t max_y);

// Persist current calibration to NVS flash
void touch_save_calibration();

// Returns true once per cooldown-gated tap. Fills sx/sy with screen coordinates.
// Consumed independently of the ButtonId return from touch_update(), so callers
// on non-main screens can use this to detect any tap.
bool touch_get_tap(int16_t& sx, int16_t& sy);
