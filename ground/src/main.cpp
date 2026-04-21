#include <Arduino.h>
#include "config.h"
#include "telemetry_packet.h"
#include "comms.h"
#include "display.h"
#include "touch.h"
#include "calibrate.h"
#include "sdlog.h"
#include "prefs.h"
#include "timesync.h"
#include "screen_menu.h"
#include "screen_settings.h"
#include "screen_units.h"
#include "screen_channel.h"

static AppState app = {};

// Screen state machine
enum AppScreen { SCREEN_MAIN, SCREEN_MENU, SCREEN_SETTINGS, SCREEN_UNITS, SCREEN_CHANNEL };
static AppScreen currentScreen = SCREEN_MAIN;

// Timing
static uint32_t last_display_ms    = 0;
static uint32_t last_touch_ms      = 0;
static uint32_t last_signal_calc_ms = 0;
static uint32_t packets_this_interval = 0;

// Confirmation dialog timeout
#define CONFIRM_TIMEOUT_MS 5000

// RGB LED helpers (active LOW)
static void led_set(bool r, bool g, bool b) {
    digitalWrite(LED_R, r ? LOW : HIGH);
    digitalWrite(LED_G, g ? LOW : HIGH);
    digitalWrite(LED_B, b ? LOW : HIGH);
}

static void led_off() { led_set(false, false, false); }

static uint32_t led_flash_ms = 0;

static void update_led() {
    uint32_t now = millis();

    // Brief blue flash on command sent
    if (led_flash_ms > 0 && now - led_flash_ms < 100) {
        led_set(false, false, true);
        return;
    }
    led_flash_ms = 0;

    if (app.signalLost) {
        // Red blink 1 Hz
        bool on = (now / 500) % 2 == 0;
        led_set(on, false, false);
    } else if (app.isArmed) {
        led_set(true, true, false); // yellow
    } else {
        led_set(false, true, false); // green
    }
}

// Navigate back to main screen
static void navigate_to_main() {
    currentScreen = SCREEN_MAIN;
    display_draw_layout();
    display_update(app);
}

static void handle_button(ButtonId btn) {
    if (btn == BTN_NONE) return;

    if (btn == BTN_ARM) {
        if (app.isArmed) {
            comms_send_command(Command::DISARM, 0);
            app.isArmed = false;
            sdlog_end_flight();
            touch_set_deploy_enabled(false);
            display_update_arm_button(false);
            display_update_deploy_button(false);
        } else {
            comms_send_command(Command::ARM, 0);
            app.isArmed = true;
            sdlog_start_flight();
            touch_set_deploy_enabled(true);
            display_update_arm_button(true);
            display_update_deploy_button(true);
        }
        led_flash_ms = millis();
    }

    if (btn == BTN_DEPLOY && !app.confirmActive) {
        app.confirmActive = true;
        app.confirmStartMs = millis();
        touch_set_confirm_mode(true);
        display_draw_confirm("DEPLOY CHUTE?");
    }

    if (btn == BTN_CONFIRM_YES && app.confirmActive) {
        uint16_t token = app.hasReceivedPacket ? app.lastPacket.seq : 0;
        comms_send_command(Command::MANUAL_DEPLOY, token);
        app.confirmActive = false;
        touch_set_confirm_mode(false);
        display_clear_confirm();
        led_flash_ms = millis();
        Serial.println("[main] Manual deploy sent!");
    }

    if (btn == BTN_CONFIRM_NO && app.confirmActive) {
        app.confirmActive = false;
        touch_set_confirm_mode(false);
        display_clear_confirm();
    }
}

static void handle_menu_tap(int16_t tx, int16_t ty) {
    // Back button: top-right header area
    if (tx >= 210 && ty < 24) {
        navigate_to_main();
        return;
    }
    if (rect_contains(MENU_BTN_SETTINGS, tx, ty)) {
        currentScreen = SCREEN_SETTINGS;
        screen_settings_draw(timesync_is_running(), timesync_is_synced());
    }
}

static void handle_settings_tap(int16_t tx, int16_t ty) {
    if (rect_contains(SETTINGS_BTN_TIME, tx, ty)) {
        timesync_restart();
        screen_settings_draw(true, timesync_is_synced());
    } else if (rect_contains(SETTINGS_BTN_CAL, tx, ty)) {
        calibrate_run();
        screen_settings_draw(timesync_is_running(), timesync_is_synced());
    } else if (rect_contains(SETTINGS_BTN_UNITS, tx, ty)) {
        currentScreen = SCREEN_UNITS;
        screen_units_draw(prefs_get_speed_unit(), prefs_get_gravity_offset());
    } else if (rect_contains(SETTINGS_BTN_CHANNEL, tx, ty)) {
        currentScreen = SCREEN_CHANNEL;
        screen_channel_enter();
    } else if (tx >= 210 && ty < 24) {
        navigate_to_main();
    }
}

static void handle_channel_tap(int16_t tx, int16_t ty) {
    if (screen_channel_handle_tap(tx, ty)) {
        currentScreen = SCREEN_SETTINGS;
        screen_settings_draw(timesync_is_running(), timesync_is_synced());
    }
}

static void handle_units_tap(int16_t tx, int16_t ty) {
    if (rect_contains(UNITS_BTN_MS, tx, ty)) {
        prefs_set_speed_unit(SPEED_MS);
        screen_units_draw(SPEED_MS, prefs_get_gravity_offset());
    } else if (rect_contains(UNITS_BTN_KMH, tx, ty)) {
        prefs_set_speed_unit(SPEED_KMH);
        screen_units_draw(SPEED_KMH, prefs_get_gravity_offset());
    } else if (rect_contains(UNITS_BTN_GRAV_OFF, tx, ty)) {
        prefs_set_gravity_offset(false);
        screen_units_draw(prefs_get_speed_unit(), false);
    } else if (rect_contains(UNITS_BTN_GRAV_ON, tx, ty)) {
        prefs_set_gravity_offset(true);
        screen_units_draw(prefs_get_speed_unit(), true);
    } else if (tx >= 210 && ty < 24) {
        currentScreen = SCREEN_SETTINGS;
        screen_settings_draw(timesync_is_running(), timesync_is_synced());
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== Water Rocket Telemetry - Ground Station ===");

    // RGB LED
    pinMode(LED_R, OUTPUT);
    pinMode(LED_G, OUTPUT);
    pinMode(LED_B, OUTPUT);
    led_off();

    // Initialize subsystems (order matters: prefs + timesync before comms)
    prefs_init();
    display_init();
    display_draw_layout();
    touch_init();

    // Calibration mode: if screen is held at boot, run the wizard
    {
        int16_t _x, _y;
        if (touch_get_raw(_x, _y)) {
            calibrate_run();
            display_draw_layout();
        }
    }

    timesync_init();  // Sets WIFI_AP_STA + starts SoftAP — must be before comms_init()
    comms_init();     // Preserves WIFI_AP_STA mode

    app.sdReady = sdlog_init();

    display_update(app);

    Serial.println("Setup complete. Waiting for telemetry...");
}

void loop() {
    uint32_t now = millis();

    // --- 1. Time sync HTTP server ---
    timesync_update();

    // --- 2. Check for new telemetry ---
    if (comms_has_new_packet()) {
        TelemetryPacket pkt = comms_get_packet();
        app.lastPacket = pkt;
        app.hasReceivedPacket = true;
        app.lastPacketMs = now;
        app.signalLost = false;
        packets_this_interval++;

        if (pkt.state == FlightState::BOOST && !app.flightActive) {
            app.flightActive = true;
            app.flightStartMs = now;
        }
        if (pkt.state == FlightState::LANDED || pkt.state == FlightState::IDLE) {
            if (app.flightActive) {
                app.flightActive = false;
            }
        }

        app.isArmed = (pkt.state != FlightState::IDLE && pkt.state != FlightState::LANDED);
        sdlog_enqueue(pkt, now, comms_get_rssi());
    }

    // --- 3. Touch input (20 Hz) ---
    if (now - last_touch_ms >= TOUCH_POLL_MS) {
        last_touch_ms = now;

        ButtonId btn = touch_update();
        int16_t tx, ty;
        bool has_tap = touch_get_tap(tx, ty);

        switch (currentScreen) {
            case SCREEN_MAIN:
                if (btn == BTN_MENU) {
                    currentScreen = SCREEN_MENU;
                    screen_menu_draw();
                } else {
                    handle_button(btn);
                }
                // Auto-dismiss confirmation dialog on timeout
                if (app.confirmActive && now - app.confirmStartMs > CONFIRM_TIMEOUT_MS) {
                    app.confirmActive = false;
                    touch_set_confirm_mode(false);
                    display_clear_confirm();
                }
                break;

            case SCREEN_MENU:
                if (has_tap) handle_menu_tap(tx, ty);
                break;

            case SCREEN_SETTINGS:
                if (has_tap) handle_settings_tap(tx, ty);
                break;

            case SCREEN_UNITS:
                if (has_tap) handle_units_tap(tx, ty);
                break;

            case SCREEN_CHANNEL:
                if (has_tap) handle_channel_tap(tx, ty);
                break;
        }
    }

    // --- 4. Display update (10 Hz, main screen only) ---
    if (now - last_display_ms >= DISPLAY_UPDATE_MS) {
        last_display_ms = now;

        app.sdReady = sdlog_is_ready();
        app.sdRows = sdlog_rows_written();
        app.sdErrors = sdlog_write_errors();

        if (currentScreen == SCREEN_MAIN) {
            bool deploy_ok = app.isArmed && !app.signalLost;
            touch_set_deploy_enabled(deploy_ok);
            display_update(app);
        } else if (currentScreen == SCREEN_SETTINGS) {
            // Refresh settings if AP just stopped (time sync completed)
            static bool prev_ap_running = true;
            bool ap_now = timesync_is_running();
            if (prev_ap_running && !ap_now) {
                screen_settings_draw(false, timesync_is_synced());
            }
            prev_ap_running = ap_now;
        }
    }

    // --- 5. SD flush ---
    sdlog_flush_some(10);

    // --- 6. Signal quality (1 Hz) ---
    if (now - last_signal_calc_ms >= SIGNAL_CALC_MS) {
        last_signal_calc_ms = now;
        app.packetsThisSecond = packets_this_interval;
        packets_this_interval = 0;

        if (app.hasReceivedPacket && now - app.lastPacketMs > SIGNAL_LOST_MS) {
            app.signalLost = true;
        }
    }

    // --- 7. RGB LED ---
    update_led();
}
