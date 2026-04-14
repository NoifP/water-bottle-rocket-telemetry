#include "screen_menu.h"
#include "config.h"
#include <TFT_eSPI.h>
#include <Arduino.h>

extern TFT_eSPI tft;

// 2-column, 3-row grid of buttons
// Left col: x=8  w=108   Right col: x=124  w=108
// Rows: y=32 y=110 y=188  h=68 each
const ButtonRect MENU_BTN_SETTINGS = {8, 32, 108, 68};

// Empty slot placeholders (hit-testing in main.cpp can ignore these)
static const ButtonRect SLOT[6] = {
    {  8,  32, 108, 68}, // [0] Settings
    {124,  32, 108, 68}, // [1] empty
    {  8, 110, 108, 68}, // [2] empty
    {124, 110, 108, 68}, // [3] empty
    {  8, 188, 108, 68}, // [4] empty
    {124, 188, 108, 68}, // [5] empty
};

static const uint16_t COL_BG      = TFT_BLACK;
static const uint16_t COL_HEADER  = 0x000F;
static const uint16_t COL_DIVIDER = 0x4208;
static const uint16_t COL_BTN     = 0x1A4B; // dark navy
static const uint16_t COL_EMPTY   = 0x2104; // very dark grey
static const uint16_t COL_TEXT    = TFT_WHITE;

static void draw_header() {
    tft.fillScreen(COL_BG);
    tft.fillRect(0, 0, SCREEN_W, 24, COL_HEADER);
    tft.setTextColor(COL_TEXT, COL_HEADER);
    tft.setTextFont(2);
    tft.setCursor(4, 4);
    tft.print("Menu");
    // Back arrow (top-right, same area as ≡ on main screen)
    // Font 1 = 6px/char, "< HOME" = 36px → start at 200 to stay within 240px screen
    tft.setTextFont(1);
    tft.setCursor(200, 8);
    tft.print("< HOME");
    tft.drawFastHLine(0, 24, SCREEN_W, COL_DIVIDER);
}

void screen_menu_draw() {
    draw_header();

    for (int i = 0; i < 6; i++) {
        const ButtonRect& r = SLOT[i];
        uint16_t col = (i == 0) ? COL_BTN : COL_EMPTY;
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 6, col);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 6, COL_DIVIDER);

        if (i == 0) {
            tft.setTextColor(COL_TEXT, col);
            tft.setTextFont(2);
            int tw = tft.textWidth("Settings");
            tft.setCursor(r.x + r.w/2 - tw/2, r.y + r.h/2 - 8);
            tft.print("Settings");
        }
        // Empty slots: draw nothing inside
    }
}
