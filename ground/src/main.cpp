#include <Arduino.h>
#include "config.h"
#include "telemetry_packet.h"
#include "comms.h"
#include "display.h"
#include "touch.h"
#include "calibrate.h"
#include "sdlog.h"

static AppState app = {};

// Timing
static uint32_t last_display_ms = 0;
static uint32_t last_touch_ms = 0;
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
        // Show confirmation dialog
        app.confirmActive = true;
        app.confirmStartMs = millis();
        touch_set_confirm_mode(true);
        display_draw_confirm("DEPLOY CHUTE?");
    }

    if (btn == BTN_CONFIRM_YES && app.confirmActive) {
        // Send manual deploy command
        uint16_t token = app.hasReceivedPacket ?
            app.lastPacket.seq : 0; // Use seq as rough token
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

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== Water Rocket Telemetry - Ground Station ===");

    // RGB LED
    pinMode(LED_R, OUTPUT);
    pinMode(LED_G, OUTPUT);
    pinMode(LED_B, OUTPUT);
    led_off();

    // Initialize subsystems
    display_init();
    display_draw_layout();
    touch_init();

    // Calibration mode: if screen is held at boot, run the wizard
    {
        int16_t _x, _y;
        if (touch_get_raw(_x, _y)) {
            calibrate_run();
            display_draw_layout(); // redraw normal UI after calibration
        }
    }

    comms_init();

    app.sdReady = sdlog_init();

    // Show initial state
    display_update(app);

    Serial.println("Setup complete. Waiting for telemetry...");
}

void loop() {
    uint32_t now = millis();

    // --- 1. Check for new telemetry ---
    if (comms_has_new_packet()) {
        TelemetryPacket pkt = comms_get_packet();
        app.lastPacket = pkt;
        app.hasReceivedPacket = true;
        app.lastPacketMs = now;
        app.signalLost = false;
        packets_this_interval++;

        // Track flight timing
        if (pkt.state == FlightState::BOOST && !app.flightActive) {
            app.flightActive = true;
            app.flightStartMs = now;
        }
        if (pkt.state == FlightState::LANDED || pkt.state == FlightState::IDLE) {
            if (app.flightActive) {
                app.flightActive = false;
            }
        }

        // Auto-update armed state from rocket
        app.isArmed = (pkt.state != FlightState::IDLE && pkt.state != FlightState::LANDED);

        // Enqueue for SD logging
        sdlog_enqueue(pkt, now, comms_get_rssi());
    }

    // --- 2. Touch input (20 Hz) ---
    if (now - last_touch_ms >= TOUCH_POLL_MS) {
        last_touch_ms = now;
        ButtonId btn = touch_update();
        handle_button(btn);

        // Auto-dismiss confirmation dialog on timeout
        if (app.confirmActive && now - app.confirmStartMs > CONFIRM_TIMEOUT_MS) {
            app.confirmActive = false;
            touch_set_confirm_mode(false);
            display_clear_confirm();
        }
    }

    // --- 3. Display update (10 Hz) ---
    if (now - last_display_ms >= DISPLAY_UPDATE_MS) {
        last_display_ms = now;

        app.sdReady = sdlog_is_ready();
        app.sdRows = sdlog_rows_written();
        app.sdErrors = sdlog_write_errors();

        // Update deploy button state
        bool deploy_ok = app.isArmed && !app.signalLost;
        touch_set_deploy_enabled(deploy_ok);

        display_update(app);
    }

    // --- 4. SD flush ---
    sdlog_flush_some(10);

    // --- 5. Signal quality (1 Hz) ---
    if (now - last_signal_calc_ms >= SIGNAL_CALC_MS) {
        last_signal_calc_ms = now;
        app.packetsThisSecond = packets_this_interval;
        packets_this_interval = 0;

        // Check for signal loss
        if (app.hasReceivedPacket && now - app.lastPacketMs > SIGNAL_LOST_MS) {
            app.signalLost = true;
        }
    }

    // --- 6. RGB LED ---
    update_led();
}
