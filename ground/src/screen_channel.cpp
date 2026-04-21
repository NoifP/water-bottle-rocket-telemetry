#include "screen_channel.h"
#include "config.h"
#include "prefs.h"
#include "comms.h"
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <Arduino.h>

extern TFT_eSPI tft;

// --- Layout ---
#define ROW_FIRST_Y  60
#define ROW_H        18
#define N_CHANNELS   13

static const ButtonRect RESCAN_BTN = {160, 28, 72, 26};

static const uint16_t COL_BG       = TFT_BLACK;
static const uint16_t COL_HEADER   = 0x000F;
static const uint16_t COL_DIVIDER  = 0x4208;
static const uint16_t COL_ROW_BG   = 0x2104; // very dark grey
static const uint16_t COL_ROW_SEL  = TFT_DARKGREEN;
static const uint16_t COL_ROW_BEST = 0x18FF; // muted blue
static const uint16_t COL_TEXT     = TFT_WHITE;
static const uint16_t COL_DIM      = 0x8410; // mid grey
static const uint16_t COL_BTN      = 0x1A4B; // dark navy

struct ChannelInfo {
    uint8_t ap_count;
    int8_t  best_rssi;   // 0 if no APs; more negative = quieter
    bool    scanned;     // false until first scan
};

static ChannelInfo s_info[N_CHANNELS];
static bool        s_scanned_once = false;
static uint8_t     s_best_ch      = 1;

static uint8_t pick_best_channel() {
    int32_t best_score = INT32_MAX;
    uint8_t best = 1;
    for (uint8_t ch = 1; ch <= N_CHANNELS; ch++) {
        const ChannelInfo& c = s_info[ch - 1];
        // Score: ap_count weighted heavily, plus strongest-RSSI penalty,
        // minus a small preference for non-overlapping channels 1/6/11.
        int32_t rssi_component = c.ap_count ? (c.best_rssi + 100) : 0; // 0..100
        int32_t score = (int32_t)c.ap_count * 20 + rssi_component;
        if (ch == 1 || ch == 6 || ch == 11) score -= 3;
        if (score < best_score) {
            best_score = score;
            best = ch;
        }
    }
    return best;
}

static void draw_rescan_button(bool dim) {
    uint16_t bg = dim ? 0x18C3 : COL_BTN;
    tft.fillRoundRect(RESCAN_BTN.x, RESCAN_BTN.y, RESCAN_BTN.w, RESCAN_BTN.h, 5, bg);
    tft.drawRoundRect(RESCAN_BTN.x, RESCAN_BTN.y, RESCAN_BTN.w, RESCAN_BTN.h, 5, COL_DIVIDER);
    tft.setTextColor(COL_TEXT, bg);
    tft.setTextFont(2);
    int tw = tft.textWidth("Rescan");
    tft.setCursor(RESCAN_BTN.x + RESCAN_BTN.w/2 - tw/2, RESCAN_BTN.y + 5);
    tft.print("Rescan");
}

static void draw_header() {
    tft.fillScreen(COL_BG);
    tft.fillRect(0, 0, SCREEN_W, 24, COL_HEADER);
    tft.setTextColor(COL_TEXT, COL_HEADER);
    tft.setTextFont(2);
    tft.setCursor(4, 4);
    tft.print("WiFi Channel");
    tft.setTextFont(1);
    tft.setCursor(200, 8);
    tft.print("< BACK");
    tft.drawFastHLine(0, 24, SCREEN_W, COL_DIVIDER);
}

static void draw_hint(const char* text) {
    tft.fillRect(0, 28, 150, 26, COL_BG);
    tft.setTextColor(COL_DIM, COL_BG);
    tft.setTextFont(1);
    tft.setCursor(6, 38);
    tft.print(text);
}

static void draw_row(uint8_t ch) {
    const ChannelInfo& c = s_info[ch - 1];
    uint8_t current = prefs_get_channel();
    int16_t y = ROW_FIRST_Y + (ch - 1) * ROW_H;

    bool is_selected = (ch == current);
    bool is_best     = s_scanned_once && (ch == s_best_ch);

    uint16_t bg;
    if (is_selected)   bg = COL_ROW_SEL;
    else if (is_best)  bg = COL_ROW_BEST;
    else               bg = COL_ROW_BG;

    tft.fillRect(4, y, 232, ROW_H - 1, bg);
    tft.setTextFont(2);
    tft.setTextColor(is_selected ? TFT_BLACK : COL_TEXT, bg);

    char buf[24];
    snprintf(buf, sizeof(buf), "Ch %2u", (unsigned)ch);
    tft.setCursor(8, y + 1);
    tft.print(buf);

    if (!c.scanned) {
        tft.setCursor(64, y + 1);
        tft.print("-");
    } else {
        snprintf(buf, sizeof(buf), "%2u APs", (unsigned)c.ap_count);
        tft.setCursor(64, y + 1);
        tft.print(buf);

        if (c.ap_count > 0) {
            snprintf(buf, sizeof(buf), "%4d dBm", (int)c.best_rssi);
            tft.setCursor(128, y + 1);
            tft.print(buf);
        }
    }

    // Marker column
    const char* mark = "";
    if (is_selected && is_best) mark = "NOW*";
    else if (is_selected)       mark = "NOW";
    else if (is_best)           mark = "BST";
    if (mark[0]) {
        tft.setCursor(200, y + 1);
        tft.print(mark);
    }
}

static void draw_rows() {
    for (uint8_t ch = 1; ch <= N_CHANNELS; ch++) {
        draw_row(ch);
    }
}

void screen_channel_rescan() {
    draw_hint("Scanning...");
    draw_rescan_button(true);

    // Clear previous results
    for (uint8_t i = 0; i < N_CHANNELS; i++) {
        s_info[i] = {0, 0, false};
    }

    // WiFi.scanNetworks(async=false, show_hidden=true) — blocking ~2-3 s.
    // This temporarily changes the radio channel; comms_reapply_channel()
    // is called below to restore our selected channel afterwards.
    int n = WiFi.scanNetworks(false, true);

    for (int i = 0; i < n; i++) {
        int32_t ch = WiFi.channel(i);
        int32_t rssi = WiFi.RSSI(i);
        if (ch < 1 || ch > N_CHANNELS) continue;
        ChannelInfo& info = s_info[ch - 1];
        info.scanned = true;
        info.ap_count++;
        if (info.ap_count == 1 || (int8_t)rssi > info.best_rssi) {
            info.best_rssi = (int8_t)rssi;
        }
    }
    // Channels with no APs are also "scanned" (quiet is data too)
    for (uint8_t i = 0; i < N_CHANNELS; i++) {
        s_info[i].scanned = true;
    }
    WiFi.scanDelete();

    s_scanned_once = true;
    s_best_ch = pick_best_channel();

    // Restore radio to the user's currently-selected channel
    comms_reapply_channel();

    draw_hint("Tap row to select");
    draw_rescan_button(false);
    draw_rows();
}

void screen_channel_redraw() {
    draw_header();
    draw_rescan_button(false);
    draw_hint(s_scanned_once ? "Tap row to select" : "Tap Rescan to start");
    draw_rows();
}

void screen_channel_enter() {
    draw_header();
    // Run an initial scan automatically so the screen isn't empty.
    screen_channel_rescan();
}

bool screen_channel_handle_tap(int16_t tx, int16_t ty) {
    // Header < BACK
    if (tx >= 200 && ty < 24) return true;

    // Rescan button
    if (rect_contains(RESCAN_BTN, tx, ty)) {
        screen_channel_rescan();
        return false;
    }

    // Channel rows
    if (tx >= 4 && tx <= 236 && ty >= ROW_FIRST_Y) {
        int row = (ty - ROW_FIRST_Y) / ROW_H;
        if (row >= 0 && row < N_CHANNELS) {
            uint8_t ch = (uint8_t)(row + 1);
            if (ch != prefs_get_channel()) {
                comms_set_channel(ch);
                draw_rows(); // re-draw so the selection marker moves
            }
        }
    }
    return false;
}
