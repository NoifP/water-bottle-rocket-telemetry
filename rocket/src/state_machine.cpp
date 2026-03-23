#include "state_machine.h"
#include "config.h"
#include "altimeter.h"
#include "parachute.h"
#include "comms.h"
#include <Arduino.h>

static FlightState state = FlightState::IDLE;
static uint8_t flags = 0;

static uint32_t armed_at_ms = 0;
static uint32_t landed_start_ms = 0;
static bool landed_timing = false;
static uint8_t boost_confirm_count = 0;
static uint32_t landed_reset_ms = 0;

// Validate command token for safety-critical commands
static bool validate_token(const CommandPacket* cmd) {
    return cmd->token == comms_get_challenge_token();
}

void state_machine_init() {
    state = FlightState::IDLE;
    flags = 0;
    boost_confirm_count = 0;
    landed_timing = false;
}

void state_machine_update(
    const SensorData& sensors,
    float altitude,
    float vspeed,
    bool apogee_detected,
    const CommandPacket* cmd
) {
    uint32_t now = millis();

    // --- Handle manual deploy command in any active flight state ---
    if (cmd && cmd->command == Command::MANUAL_DEPLOY && state != FlightState::IDLE) {
        if (validate_token(cmd)) {
            if (parachute_deploy(state)) {
                flags |= FLAG_MANUAL_DEPLOY;
                flags |= FLAG_CHUTE_DEPLOYED;
                if (state == FlightState::ARMED || state == FlightState::BOOST ||
                    state == FlightState::COAST) {
                    state = FlightState::DESCENT;
                }
                Serial.println("[state] Manual deploy triggered!");
            }
        }
    }

    switch (state) {

    case FlightState::IDLE:
        flags = 0;
        if (cmd && cmd->command == Command::ARM) {
            // ARM -- no token required for arming (token is generated here)
            comms_new_challenge_token();
            altimeter_set_baseline(sensors.pressure_pa);
            parachute_arm();
            armed_at_ms = now;
            boost_confirm_count = 0;
            state = FlightState::ARMED;
            Serial.println("[state] IDLE -> ARMED");
        }
        break;

    case FlightState::ARMED:
        // Check for disarm command
        if (cmd && cmd->command == Command::DISARM) {
            parachute_disarm();
            altimeter_reset();
            parachute_reset();
            state = FlightState::IDLE;
            Serial.println("[state] ARMED -> IDLE (disarm)");
            break;
        }
        // Check for armed timeout
        if (now - armed_at_ms > ARMED_TIMEOUT_MS) {
            parachute_disarm();
            altimeter_reset();
            parachute_reset();
            state = FlightState::IDLE;
            Serial.println("[state] ARMED -> IDLE (timeout)");
            break;
        }
        // Check for boost (high acceleration)
        if (sensors.accel_magnitude > BOOST_ACCEL_THRESHOLD) {
            boost_confirm_count++;
            if (boost_confirm_count >= BOOST_CONFIRM_SAMPLES) {
                state = FlightState::BOOST;
                Serial.println("[state] ARMED -> BOOST");
            }
        } else {
            boost_confirm_count = 0;
        }
        break;

    case FlightState::BOOST:
        // Thrust ended when acceleration drops
        if (sensors.accel_magnitude < COAST_ACCEL_THRESHOLD) {
            state = FlightState::COAST;
            Serial.println("[state] BOOST -> COAST");
        }
        break;

    case FlightState::COAST:
        // Check for apogee
        if (apogee_detected) {
            state = FlightState::APOGEE;
            flags |= FLAG_APOGEE_DETECTED;
            parachute_deploy(state);
            flags |= FLAG_CHUTE_DEPLOYED;
            Serial.println("[state] COAST -> APOGEE (deploying chute)");
            // Immediately transition to descent
            state = FlightState::DESCENT;
            Serial.println("[state] APOGEE -> DESCENT");
        }
        break;

    case FlightState::APOGEE:
        // Should not stay here -- transitions immediately to DESCENT
        state = FlightState::DESCENT;
        break;

    case FlightState::DESCENT:
        // Check for landing
        if (fabsf(vspeed) < LANDED_SPEED_THRESHOLD &&
            altitude < LANDED_ALT_THRESHOLD) {
            if (!landed_timing) {
                landed_timing = true;
                landed_start_ms = now;
            } else if (now - landed_start_ms >= LANDED_CONFIRM_MS) {
                state = FlightState::LANDED;
                landed_reset_ms = now;
                landed_timing = false;
                Serial.println("[state] DESCENT -> LANDED");
            }
        } else {
            landed_timing = false;
        }
        break;

    case FlightState::LANDED:
        // Auto-return to IDLE after 30 seconds, or on DISARM command
        if (cmd && cmd->command == Command::DISARM) {
            altimeter_reset();
            parachute_reset();
            state = FlightState::IDLE;
            Serial.println("[state] LANDED -> IDLE (disarm)");
        } else if (now - landed_reset_ms > 30000) {
            altimeter_reset();
            parachute_reset();
            state = FlightState::IDLE;
            Serial.println("[state] LANDED -> IDLE (auto-reset)");
        }
        break;
    }
}

FlightState state_machine_get_state() {
    return state;
}

uint8_t state_machine_get_flags() {
    return flags;
}
