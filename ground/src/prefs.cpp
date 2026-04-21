#include "prefs.h"
#include "config.h"
#include <Preferences.h>
#include <time.h>
#include <stdlib.h>  // setenv
#include <stdio.h>

static SpeedUnit s_speed_unit    = SPEED_MS;
static bool      s_gravity_offset = false;
static int16_t   s_tz_offset     = 0; // minutes west of UTC
static uint8_t   s_channel       = ESPNOW_CHANNEL_DEFAULT;

static void apply_tz(int16_t minutes) {
    int hours = minutes / 60;
    int mins  = minutes % 60;
    if (mins < 0) mins = -mins;
    char tz_str[16];
    if (mins == 0)
        snprintf(tz_str, sizeof(tz_str), "UTC%+d", hours);
    else
        snprintf(tz_str, sizeof(tz_str), "UTC%+d:%02d", hours, mins);
    setenv("TZ", tz_str, 1);
    tzset();
}

void prefs_init() {
    Preferences p;
    p.begin("display", true); // read-only
    s_speed_unit    = (SpeedUnit)p.getUChar("spd_unit", (uint8_t)SPEED_MS);
    s_gravity_offset = p.getBool("grav_off", false);
    s_tz_offset     = p.getShort("tz_offset", 0);
    p.end();

    Preferences r;
    r.begin("radio", true);
    s_channel = r.getUChar("espnow_ch", ESPNOW_CHANNEL_DEFAULT);
    r.end();
    if (s_channel < 1 || s_channel > 13) s_channel = ESPNOW_CHANNEL_DEFAULT;

    apply_tz(s_tz_offset);
}

SpeedUnit prefs_get_speed_unit()     { return s_speed_unit; }
bool      prefs_get_gravity_offset() { return s_gravity_offset; }
int16_t   prefs_get_tz_offset()      { return s_tz_offset; }

void prefs_set_speed_unit(SpeedUnit u) {
    s_speed_unit = u;
    Preferences p;
    p.begin("display", false);
    p.putUChar("spd_unit", (uint8_t)u);
    p.end();
}

void prefs_set_gravity_offset(bool enabled) {
    s_gravity_offset = enabled;
    Preferences p;
    p.begin("display", false);
    p.putBool("grav_off", enabled);
    p.end();
}

void prefs_set_tz_offset(int16_t minutes) {
    s_tz_offset = minutes;
    apply_tz(minutes);
    Preferences p;
    p.begin("display", false);
    p.putShort("tz_offset", minutes);
    p.end();
}

uint8_t prefs_get_channel() { return s_channel; }

void prefs_set_channel(uint8_t ch) {
    if (ch < 1 || ch > 13) return;
    s_channel = ch;
    Preferences r;
    r.begin("radio", false);
    r.putUChar("espnow_ch", ch);
    r.end();
}
