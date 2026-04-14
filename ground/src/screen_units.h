#pragma once
#include "touch.h"
#include "prefs.h"

// Draws the display units screen with current selections highlighted.
void screen_units_draw(SpeedUnit unit, bool gravity_offset);

// Hit-test rects for main.cpp
extern const ButtonRect UNITS_BTN_MS;
extern const ButtonRect UNITS_BTN_KMH;
extern const ButtonRect UNITS_BTN_GRAV_OFF;
extern const ButtonRect UNITS_BTN_GRAV_ON;
// Back to settings: tap header area sx >= 210, sy < 24
