#pragma once
#include "touch.h"

// Draw the channel scan screen from the current scan results (or trigger a
// fresh scan first if none have been taken yet).
void screen_channel_enter();

// Re-draw the current state of the screen without re-scanning.
void screen_channel_redraw();

// Perform a WiFi scan and redraw. Blocking (~3s).
void screen_channel_rescan();

// Handle a tap on this screen. Returns true if the caller should navigate
// back to the Settings screen (user tapped "< BACK"), false otherwise.
bool screen_channel_handle_tap(int16_t tx, int16_t ty);
