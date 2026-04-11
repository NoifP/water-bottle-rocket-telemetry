#include <Arduino.h>
#include "config.h"
#include "telemetry_packet.h"
#include "sensors.h"
#include "altimeter.h"
#include "parachute.h"
#include "comms.h"
#include "state_machine.h"

static uint16_t seq_counter = 0;
static uint32_t last_loop_us = 0;
static uint32_t last_command_ms = 0;
static bool low_power_mode = false;

// --- LED status ---
static uint32_t led_last_toggle_ms = 0;
static bool led_on = false;

static void update_status_led() {
    uint32_t now = millis();
    FlightState s = state_machine_get_state();
    uint32_t interval = 0;

    switch (s) {
        case FlightState::IDLE:
            interval = low_power_mode ? 2000 : 1000;
            break;
        case FlightState::ARMED:
            interval = 200;
            break;
        case FlightState::BOOST:
        case FlightState::COAST:
        case FlightState::LANDED:
            // Solid ON
            digitalWrite(PIN_LED, HIGH);
            return;
        case FlightState::APOGEE:
            interval = 100; // rapid flash
            break;
        case FlightState::DESCENT:
            interval = 250;
            break;
    }

    if (now - led_last_toggle_ms >= interval) {
        led_last_toggle_ms = now;
        led_on = !led_on;
        digitalWrite(PIN_LED, led_on ? HIGH : LOW);
    }
}

// --- Battery voltage ---
static float read_battery_voltage() {
    int raw = analogRead(PIN_BATTERY_ADC);
    float voltage = (raw / 4095.0f) * 3.3f * BATTERY_DIVIDER_RATIO;
    return voltage;
}

void setup() {
    Serial.begin(115200);
    { unsigned long t = millis(); while (!Serial && millis() - t < 10000); } // Wait up to 10s for USB CDC
    Serial.println("\n=== Water Rocket Telemetry - Rocket ===");

    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);

    analogReadResolution(12);

    if (!sensors_init()) {
        Serial.println("WARNING: No barometer found! Altitude will not work.");
    }

    altimeter_init();
    parachute_init();
    comms_init();
    state_machine_init();

    last_loop_us = micros();
    last_command_ms = millis();

    Serial.println("Setup complete. Entering main loop.");
}

void loop() {
    uint32_t now_us = micros();
    uint32_t now_ms = millis();

    // Determine loop interval based on power mode
    uint32_t interval = low_power_mode ? IDLE_LOOP_INTERVAL_US : LOOP_INTERVAL_US;

    if (now_us - last_loop_us < interval) return;

    float dt = (now_us - last_loop_us) / 1e6f;
    last_loop_us = now_us;

    // --- 1. Read sensors ---
    SensorData sensors;
    sensors_read(sensors);

    // --- 2. Update altimeter ---
    if (sensors.baro_ok) {
        altimeter_update(sensors.pressure_pa, sensors.accel_magnitude, dt);
    }

    // --- 3. Check for commands ---
    CommandPacket cmd;
    bool has_cmd = comms_get_command(cmd);
    if (has_cmd) {
        last_command_ms = now_ms;
    }

    // --- 4. Update state machine ---
    state_machine_update(
        sensors,
        altimeter_get_altitude(),
        altimeter_get_vspeed(),
        altimeter_apogee_detected(),
        has_cmd ? &cmd : nullptr
    );

    // --- 5. Power management ---
    FlightState current_state = state_machine_get_state();
    if (current_state == FlightState::IDLE) {
        low_power_mode = (now_ms - last_command_ms > IDLE_TIMEOUT_MS);
    } else {
        low_power_mode = false;
    }

    // --- 6. Build and send telemetry ---
    TelemetryPacket pkt = {};
    pkt.magic = TELEMETRY_MAGIC;
    pkt.version = PROTOCOL_VERSION;
    pkt.seq = seq_counter++;
    pkt.timestamp_ms = now_ms;

    pkt.pressure_pa = sensors.pressure_pa;
    pkt.altitude_m = altimeter_get_altitude();
    pkt.vertical_speed = altimeter_get_vspeed();

    pkt.accel_x = sensors.accel_x;
    pkt.accel_y = sensors.accel_y;
    pkt.accel_z = sensors.accel_z;
    pkt.gyro_x = sensors.gyro_x;
    pkt.gyro_y = sensors.gyro_y;
    pkt.gyro_z = sensors.gyro_z;
    pkt.accel_magnitude = sensors.accel_magnitude;

    pkt.temperature_c = sensors.temperature_c;
    pkt.humidity_pct = sensors.humidity_pct;

    pkt.state = current_state;
    pkt.flags = state_machine_get_flags();

    // Low battery flag
    float batt_v = read_battery_voltage();
    pkt.battery_v = batt_v;
    if (batt_v < BATTERY_LOW_V && batt_v > 0.5f) {
        pkt.flags |= FLAG_LOW_BATTERY;
    }

    pkt.max_altitude_m = altimeter_get_max_alt();

    // Compute checksum (over all bytes except the checksum field itself)
    pkt.checksum = compute_checksum(&pkt, sizeof(TelemetryPacket) - 1);

    comms_send_telemetry(pkt);

    // --- 7. Update LED ---
    update_status_led();
}
