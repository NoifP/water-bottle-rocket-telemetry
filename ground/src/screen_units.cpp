#include "screen_units.h"
#include "config.h"
#include <TFT_eSPI.h>
#include <Arduino.h>

extern TFT_eSPI tft;

// Left toggle: x=8  w=108   Right toggle: x=124  w=108   h=44
const ButtonRect UNITS_BTN_MS       = {  8,  74, 108, 44};
const ButtonRect UNITS_BTN_KMH      = {124,  74, 108, 44};
const ButtonRect UNITS_BTN_GRAV_OFF = {  8, 164, 108, 44};
const ButtonRect UNITS_BTN_GRAV_ON  = {124, 164, 108, 44};
const ButtonRect UNITS_BTN_OK       = { 70, 268, 100, 44};

static const uint16_t COL_BG       = TFT_BLACK;
static const uint16_t COL_HEADER   = 0x000F;
static const uint16_t COL_DIVIDER  = 0x4208;
static const uint16_t COL_SEL      = TFT_DARKGREEN;
static const uint16_t COL_UNSEL_BG = 0x2104;
static const uint16_t COL_TEXT     = TFT_WHITE;
static const uint16_t COL_LABEL    = TFT_CYAN;

static void draw_toggle(const ButtonRect& r, const char* label, bool selected) {
    uint16_t bg  = selected ? COL_SEL : COL_UNSEL_BG;
    uint16_t tc  = selected ? TFT_BLACK : COL_TEXT;
    tft.fillRoundRect(r.x, r.y, r.w, r.h, 6, bg);
    if (!selected) tft.drawRoundRect(r.x, r.y, r.w, r.h, 6, COL_DIVIDER);
    tft.setTextColor(tc, bg);
    tft.setTextFont(2);
    int tw = tft.textWidth(label);
    tft.setCursor(r.x + r.w/2 - tw/2, r.y + r.h/2 - 8);
    tft.print(label);
}

void screen_units_draw(SpeedUnit unit, bool gravity_offset) {
    tft.fillScreen(COL_BG);
    tft.fillRect(0, 0, SCREEN_W, 24, COL_HEADER);
    tft.setTextColor(COL_TEXT, COL_HEADER);
    tft.setTextFont(2);
    tft.setCursor(4, 4);
    tft.print("Display Units");
    tft.setTextFont(1);
    tft.setCursor(200, 8);
    tft.print("< BACK");
    tft.setTextFont(2);
    tft.drawFastHLine(0, 24, SCREEN_W, COL_DIVIDER);

    // --- Speed ---
    tft.setTextColor(COL_LABEL, COL_BG);
    tft.setTextFont(2);
    tft.setCursor(8, 52);
    tft.print("Speed");

    draw_toggle(UNITS_BTN_MS,  "m/s",   unit == SPEED_MS);
    draw_toggle(UNITS_BTN_KMH, "km/h",  unit == SPEED_KMH);

    // --- Accel offset ---
    tft.setTextColor(COL_LABEL, COL_BG);
    tft.setTextFont(2);
    tft.setCursor(8, 142);
    tft.print("Accel offset (-1g)");

    draw_toggle(UNITS_BTN_GRAV_OFF, "OFF", !gravity_offset);
    draw_toggle(UNITS_BTN_GRAV_ON,  "ON",   gravity_offset);

    // Small description
    tft.setTextColor(0x8410, COL_BG); // mid grey
    tft.setTextFont(1);
    tft.setCursor(8, 218);
    tft.print("ON = subtract 1g (shows ~0g at rest)");

}
