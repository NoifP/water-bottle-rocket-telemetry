#include "display.h"
#include "config.h"
#include "touch.h"
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI(); // non-static so calibrate.cpp can extern it

// Colors
#define COL_BG       TFT_BLACK
#define COL_TEXT     TFT_WHITE
#define COL_LABEL    TFT_CYAN
#define COL_DIVIDER  0x4208  // dark grey
#define COL_HEADER   0x000F  // very dark blue

// State badge colors
static uint16_t state_color(FlightState s) {
    switch (s) {
        case FlightState::IDLE:    return TFT_DARKGREY;
        case FlightState::ARMED:   return TFT_YELLOW;
        case FlightState::BOOST:   return TFT_ORANGE;
        case FlightState::COAST:   return TFT_ORANGE;
        case FlightState::APOGEE:  return TFT_GREEN;
        case FlightState::DESCENT: return TFT_CYAN;
        case FlightState::LANDED:  return TFT_WHITE;
        default:                   return TFT_RED;
    }
}

// Signal quality color
static uint16_t signal_color(uint32_t pps) {
    if (pps > 35) return TFT_GREEN;
    if (pps > 20) return TFT_YELLOW;
    return TFT_RED;
}

// Helper: draw a value field (clear old value, draw new)
static void draw_field(int y, const char* label, const char* value) {
    // Label
    tft.setTextColor(COL_LABEL, COL_BG);
    tft.setTextFont(2);
    tft.setCursor(8, y);
    tft.print(label);

    // Value (right-aligned)
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.setTextFont(4);
    // Clear value area
    tft.fillRect(90, y - 2, 145, 28, COL_BG);
    tft.setCursor(90, y);
    tft.print(value);
}

// Helper: draw signal bars
static void draw_signal_bars(uint32_t pps) {
    int bars = 0;
    if (pps > 5)  bars = 1;
    if (pps > 15) bars = 2;
    if (pps > 25) bars = 3;
    if (pps > 35) bars = 4;
    if (pps > 45) bars = 5;

    uint16_t col = signal_color(pps);
    int x = 170;
    int y = 4;
    int bar_w = 6;
    int gap = 2;

    for (int i = 0; i < 5; i++) {
        int bar_h = 4 + i * 3;
        int bar_y = y + (16 - bar_h);
        uint16_t c = (i < bars) ? col : COL_DIVIDER;
        tft.fillRect(x + i * (bar_w + gap), bar_y, bar_w, bar_h, c);
    }
}

void display_init() {
    tft.init();
    tft.setRotation(0); // Portrait: 240x320
    tft.fillScreen(COL_BG);

    // Turn on backlight
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);

    Serial.println("[display] TFT initialized");
}

void display_draw_layout() {
    tft.fillScreen(COL_BG);

    // Header bar
    tft.fillRect(0, 0, SCREEN_W, 24, COL_HEADER);
    tft.setTextColor(COL_TEXT, COL_HEADER);
    tft.setTextFont(2);
    tft.setCursor(4, 4);
    tft.print("ROCKET TELEMETRY");

    // Divider after header
    tft.drawFastHLine(0, 24, SCREEN_W, COL_DIVIDER);

    // Divider before buttons
    tft.drawFastHLine(0, 224, SCREEN_W, COL_DIVIDER);

    // Initial button states
    display_update_arm_button(false);
    display_update_deploy_button(false);

    // Status footer background
    tft.fillRect(0, 278, SCREEN_W, 42, COL_BG);
}

void display_update(const AppState& state) {
    char buf[32];

    if (!state.hasReceivedPacket || state.signalLost) {
        // No signal state
        draw_field(35, "ALT", "--");
        draw_field(73, "SPD", "--");
        draw_field(111, "ACC", "--");
        draw_field(149, "TIME", "--");

        // State badge
        tft.fillRect(8, 190, 224, 28, COL_BG);
        tft.setTextColor(TFT_RED, COL_BG);
        tft.setTextFont(4);
        tft.setCursor(30, 192);
        tft.print(state.hasReceivedPacket ? "SIGNAL LOST" : "WAITING...");

        // Signal bars
        draw_signal_bars(0);

        // Footer: touch coordinates (always visible for calibration diagnostics)
        tft.fillRect(0, 279, SCREEN_W, 41, COL_BG);
        tft.setTextColor(COL_LABEL, COL_BG);
        tft.setTextFont(1);
        {
            int16_t tx, ty, rx, ry;
            touch_get_last_pos(tx, ty);
            touch_get_last_raw(rx, ry);
            char tbuf[40];
            if (tx < 0) {
                snprintf(tbuf, sizeof(tbuf), "T:--,--  R:--,--");
            } else {
                snprintf(tbuf, sizeof(tbuf), "T:%d,%d  R:%d,%d", tx, ty, rx, ry);
            }
            tft.setCursor(4, 283);
            tft.print(tbuf);
        }
        return;
    }

    const TelemetryPacket& p = state.lastPacket;

    // Altitude
    snprintf(buf, sizeof(buf), "%.1f m", p.altitude_m);
    draw_field(35, "ALT", buf);

    // Speed
    snprintf(buf, sizeof(buf), "%.1f m/s", p.vertical_speed);
    draw_field(73, "SPD", buf);

    // Acceleration (in g)
    snprintf(buf, sizeof(buf), "%.1f g", p.accel_magnitude / 9.81f);
    draw_field(111, "ACC", buf);

    // Flight time
    if (state.flightActive) {
        float t = (millis() - state.flightStartMs) / 1000.0f;
        snprintf(buf, sizeof(buf), "%.2f s", t);
    } else {
        snprintf(buf, sizeof(buf), "0.00 s");
    }
    draw_field(149, "TIME", buf);

    // State badge
    tft.fillRect(8, 190, 224, 28, COL_BG);
    uint16_t sc = state_color(p.state);
    tft.fillRoundRect(70, 190, 100, 26, 4, sc);
    // Text color: black on light badges, white on dark
    uint16_t tc = (p.state == FlightState::IDLE || p.state == FlightState::DESCENT) ?
                   COL_TEXT : TFT_BLACK;
    tft.setTextColor(tc, sc);
    tft.setTextFont(2);
    const char* sstr = flight_state_str(p.state);
    int sw = tft.textWidth(sstr);
    tft.setCursor(120 - sw / 2, 194);
    tft.print(sstr);

    // Signal bars
    draw_signal_bars(state.packetsThisSecond);

    // Signal text
    tft.fillRect(210, 4, 30, 16, COL_HEADER);
    tft.setTextColor(signal_color(state.packetsThisSecond), COL_HEADER);
    tft.setTextFont(1);
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)state.packetsThisSecond);
    tft.setCursor(215, 8);
    tft.print(buf);

    // Status footer
    tft.fillRect(0, 279, SCREEN_W, 41, COL_BG);
    tft.setTextColor(COL_LABEL, COL_BG);
    tft.setTextFont(1);

    // Last touch coordinates (mapped then raw) for calibration diagnostics
    {
        int16_t tx, ty, rx, ry;
        touch_get_last_pos(tx, ty);
        touch_get_last_raw(rx, ry);
        char tbuf[40];
        if (tx < 0) {
            snprintf(tbuf, sizeof(tbuf), "T:--,--  R:--,--");
        } else {
            snprintf(tbuf, sizeof(tbuf), "T:%d,%d  R:%d,%d", tx, ty, rx, ry);
        }
        tft.setCursor(4, 283);
        tft.print(tbuf);
    }

    // SD status
    tft.setCursor(4, 300);
    if (state.sdReady) {
        snprintf(buf, sizeof(buf), "SD: OK %lu rows", (unsigned long)state.sdRows);
    } else {
        snprintf(buf, sizeof(buf), "SD: NO CARD");
    }
    tft.print(buf);

    // Packet rate
    snprintf(buf, sizeof(buf), "PKT: %lu/50 Hz", (unsigned long)state.packetsThisSecond);
    tft.setCursor(150, 300);
    tft.print(buf);
}

void display_draw_confirm(const char* message) {
    // Semi-transparent overlay (just fill dark)
    tft.fillRect(20, 120, 200, 100, TFT_BLACK);
    tft.drawRect(20, 120, 200, 100, TFT_WHITE);

    // Message
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextFont(2);
    int mw = tft.textWidth(message);
    tft.setCursor(120 - mw / 2, 135);
    tft.print(message);

    // YES button
    tft.fillRoundRect(40, 175, 70, 36, 4, TFT_RED);
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.setTextFont(2);
    tft.setCursor(58, 183);
    tft.print("YES");

    // NO button
    tft.fillRoundRect(130, 175, 70, 36, 4, TFT_DARKGREEN);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREEN);
    tft.setTextFont(2);
    tft.setCursor(152, 183);
    tft.print("NO");
}

void display_clear_confirm() {
    // Redraw the area that was covered by the dialog
    tft.fillRect(20, 120, 200, 100, COL_BG);
}

void display_update_arm_button(bool is_armed) {
    uint16_t bg = is_armed ? TFT_RED : TFT_DARKGREEN;
    const char* label = is_armed ? "DISARM" : "ARM";

    tft.fillRoundRect(8, 230, 108, 44, 6, bg);
    tft.setTextColor(TFT_WHITE, bg);
    tft.setTextFont(2);
    int lw = tft.textWidth(label);
    tft.setCursor(62 - lw / 2, 244);
    tft.print(label);
}

void display_update_deploy_button(bool enabled) {
    uint16_t bg = enabled ? TFT_RED : TFT_DARKGREY;
    uint16_t tc = enabled ? TFT_WHITE : 0x7BEF; // lighter grey text when disabled

    tft.fillRoundRect(124, 230, 108, 44, 6, bg);
    tft.setTextColor(tc, bg);
    tft.setTextFont(2);
    tft.setCursor(137, 237);
    tft.print("DEPLOY");
    tft.setCursor(140, 254);
    tft.print("CHUTE");
}
