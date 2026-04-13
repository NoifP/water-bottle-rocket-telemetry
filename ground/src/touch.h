#pragma once

#include <stdint.h>

// Button IDs
enum ButtonId {
    BTN_NONE = 0,
    BTN_ARM,
    BTN_DEPLOY,
    BTN_CONFIRM_YES,
    BTN_CONFIRM_NO,
};

// Initialize touchscreen
void touch_init();

// Poll touch and check for button presses. Call at 20 Hz.
// Returns the ButtonId of a pressed button, or BTN_NONE.
ButtonId touch_update();

// Set whether the deploy button is enabled (greyed out if disabled)
void touch_set_deploy_enabled(bool enabled);

// Set whether we're showing the confirmation dialog
void touch_set_confirm_mode(bool active);

// Get raw touch coordinates (for calibration)
bool touch_get_raw(int16_t& x, int16_t& y);

// Last mapped screen coordinates from any touch (x/y = -1 if never touched)
void touch_get_last_pos(int16_t& x, int16_t& y);

// Last raw ADC coordinates from any touch (x/y = -1 if never touched)
void touch_get_last_raw(int16_t& x, int16_t& y);

// Override calibration min/max values at runtime
void touch_set_calibration(int16_t min_x, int16_t max_x, int16_t min_y, int16_t max_y);

// Persist current calibration to NVS flash
void touch_save_calibration();
