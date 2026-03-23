#include "parachute.h"
#include "config.h"
#include <ESP32Servo.h>

static Servo servo;
static bool deployed = false;
static bool armed = false;

void parachute_init() {
    // Briefly attach to set locked position, then detach
    // This ensures the servo is in the right position on boot
    servo.attach(PIN_SERVO);
    servo.write(SERVO_LOCKED_ANGLE);
    delay(500);
    servo.detach();
    deployed = false;
    armed = false;
    Serial.println("[parachute] Init -- locked and detached");
}

void parachute_arm() {
    servo.attach(PIN_SERVO);
    servo.write(SERVO_LOCKED_ANGLE);
    armed = true;
    deployed = false;
    Serial.println("[parachute] Armed -- servo attached at locked position");
}

bool parachute_deploy(FlightState current_state) {
    // Safety: never deploy in IDLE
    if (current_state == FlightState::IDLE) {
        Serial.println("[parachute] BLOCKED -- cannot deploy in IDLE state");
        return false;
    }

    // Already deployed -- no-op
    if (deployed) {
        return false;
    }

    // Attach if not already
    if (!armed) {
        servo.attach(PIN_SERVO);
    }

    servo.write(SERVO_DEPLOY_ANGLE);
    deployed = true;
    Serial.println("[parachute] DEPLOYED!");

    // Detach after a delay to save power (servo holds position mechanically)
    delay(SERVO_DETACH_DELAY_MS);
    servo.detach();
    armed = false;

    return true;
}

void parachute_disarm() {
    if (armed) {
        servo.write(SERVO_LOCKED_ANGLE);
        delay(500);
        servo.detach();
        armed = false;
        Serial.println("[parachute] Disarmed -- locked and detached");
    }
}

bool parachute_is_deployed() {
    return deployed;
}

void parachute_reset() {
    deployed = false;
    armed = false;
    // Ensure servo is detached
    servo.detach();
}
