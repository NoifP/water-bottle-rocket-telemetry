#include "prefs.h"
#include "config.h"
#include <Preferences.h>

static uint8_t s_channel = ESPNOW_CHANNEL_DEFAULT;

void prefs_init() {
    Preferences p;
    p.begin("rocket", true); // read-only
    s_channel = p.getUChar("ch", ESPNOW_CHANNEL_DEFAULT);
    p.end();
    if (s_channel < 1 || s_channel > 13) s_channel = ESPNOW_CHANNEL_DEFAULT;
}

uint8_t prefs_get_channel() { return s_channel; }

void prefs_set_channel(uint8_t ch) {
    if (ch < 1 || ch > 13) return;
    s_channel = ch;
    Preferences p;
    p.begin("rocket", false);
    p.putUChar("ch", ch);
    p.end();
}
