#pragma once
#include "touch.h"

// Draws the settings screen. ap_running / time_synced control button appearance.
void screen_settings_draw(bool ap_running, bool time_synced);

// Hit-test rects for main.cpp
extern const ButtonRect SETTINGS_BTN_TIME;
extern const ButtonRect SETTINGS_BTN_CAL;
extern const ButtonRect SETTINGS_BTN_UNITS;
extern const ButtonRect SETTINGS_BTN_CHANNEL;
// Back to main: tap header area sx >= 210, sy < 24 (same as menu screen)
