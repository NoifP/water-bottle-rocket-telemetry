#pragma once
#include "touch.h"

void screen_menu_draw();

// Hit-test rects for main.cpp navigation
// Back: any tap with sx >= 210 && sy < 24 (header area)
extern const ButtonRect MENU_BTN_SETTINGS;
