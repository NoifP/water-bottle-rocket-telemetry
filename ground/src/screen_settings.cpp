#include "screen_settings.h"
#include "config.h"
#include "prefs.h"
#include <TFT_eSPI.h>
#include <time.h>
#include <Arduino.h>

extern TFT_eSPI tft;

// Full-width buttons, stacked vertically
const ButtonRect SETTINGS_BTN_TIME    = {8,  40, 224, 52};
const ButtonRect SETTINGS_BTN_CAL     = {8, 104, 224, 44};
const ButtonRect SETTINGS_BTN_UNITS   = {8, 156, 224, 44};
const ButtonRect SETTINGS_BTN_CHANNEL = {8, 208, 224, 44};

static const uint16_t COL_BG      = TFT_BLACK;
static const uint16_t COL_HEADER  = 0x000F;
static const uint16_t COL_DIVIDER = 0x4208;
static const uint16_t COL_BTN     = 0x1A4B; // dark navy
static const uint16_t COL_TEXT    = TFT_WHITE;
static const uint16_t COL_DIM     = 0x4208;

static void draw_full_btn(const ButtonRect& r, uint16_t bg,
                          const char* label, const char* sub = nullptr) {
    tft.fillRoundRect(r.x, r.y, r.w, r.h, 6, bg);
    tft.setTextColor(COL_TEXT, bg);
    tft.setTextFont(2);
    int16_t label_y = sub ? r.y + 8 : r.y + r.h/2 - 8;
    tft.setCursor(r.x + 10, label_y);
    tft.print(label);
    if (sub && sub[0]) {
        tft.setTextFont(1);
        tft.setTextColor(0xC618, bg); // light grey sub-text
        tft.setCursor(r.x + 10, r.y + 32);
        tft.print(sub);
    }
}

void screen_settings_draw(bool ap_running, bool time_synced) {
    tft.fillScreen(COL_BG);
    tft.fillRect(0, 0, SCREEN_W, 24, COL_HEADER);
    tft.setTextColor(COL_TEXT, COL_HEADER);
    tft.setTextFont(2);
    tft.setCursor(4, 4);
    tft.print("Settings");
    // Back arrow (same style and position as menu screen)
    tft.setTextFont(1);
    tft.setCursor(200, 8);
    tft.print("< HOME");
    tft.setTextFont(2); // restore for buttons below
    tft.drawFastHLine(0, 24, SCREEN_W, COL_DIVIDER);

    // --- Set Time button ---
    uint16_t time_col;
    char time_label[28], time_sub[32];

    if (ap_running) {
        time_col = TFT_ORANGE;
        strcpy(time_label, "Sync: AP Active");
        strcpy(time_sub,   "Connect: WaterRocketGS");
    } else if (time_synced) {
        time_col = TFT_DARKGREEN;
        strcpy(time_label, "Resync Time");
        time_t now = time(NULL);
        struct tm* t = localtime(&now);
        strftime(time_sub, sizeof(time_sub), "%Y-%m-%d %H:%M:%S", t);
    } else {
        time_col = COL_BTN;
        strcpy(time_label, "Set Time");
        strcpy(time_sub,   "Starts WiFi AP for sync");
    }
    draw_full_btn(SETTINGS_BTN_TIME, time_col, time_label, time_sub);

    // --- Calibrate Touch ---
    draw_full_btn(SETTINGS_BTN_CAL, COL_BTN, "Calibrate Touch (hold)");

    // --- Display Units ---
    draw_full_btn(SETTINGS_BTN_UNITS, COL_BTN, "Display Units");

    // --- WiFi Channel ---
    char chan_sub[24];
    snprintf(chan_sub, sizeof(chan_sub), "Current: Ch %u", (unsigned)prefs_get_channel());
    draw_full_btn(SETTINGS_BTN_CHANNEL, COL_BTN, "WiFi Channel", chan_sub);
}
