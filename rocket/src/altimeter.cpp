#include "altimeter.h"
#include "config.h"
#include <math.h>

static float baseline_pa = 101325.0f;
static float altitude = 0.0f;
static float vspeed = 0.0f;
static float max_alt = 0.0f;
static float prev_altitude = 0.0f;

static bool apogee_triggered = false;

// Apogee detection timers
static uint32_t alt_drop_start_ms = 0;
static bool alt_dropping = false;
static uint32_t freefall_start_ms = 0;
static bool in_freefall = false;

static float pressure_to_altitude(float pressure_pa, float baseline_pa) {
    // Barometric formula
    return 44330.0f * (1.0f - powf(pressure_pa / baseline_pa, 0.1903f));
}

void altimeter_init() {
    altimeter_reset();
}

void altimeter_set_baseline(float pressure_pa) {
    baseline_pa = pressure_pa;
    altitude = 0.0f;
    prev_altitude = 0.0f;
    vspeed = 0.0f;
    max_alt = 0.0f;
    apogee_triggered = false;
    alt_dropping = false;
    in_freefall = false;
}

void altimeter_update(float pressure_pa, float accel_magnitude, float dt) {
    if (dt <= 0.0f) return;

    // Compute altitude AGL
    float raw_alt = pressure_to_altitude(pressure_pa, baseline_pa);

    // Exponential smoothing on altitude
    altitude = 0.7f * raw_alt + 0.3f * altitude;

    // Vertical speed with exponential smoothing
    float raw_vspeed = (altitude - prev_altitude) / dt;
    vspeed = VSPEED_ALPHA * raw_vspeed + (1.0f - VSPEED_ALPHA) * vspeed;
    prev_altitude = altitude;

    // Track peak altitude
    if (altitude > max_alt) {
        max_alt = altitude;
    }

    // --- Apogee detection (dual criteria) ---
    if (apogee_triggered) return;

    uint32_t now = millis();

    // Criterion 1: barometric -- altitude dropping from peak
    if (max_alt - altitude > APOGEE_ALT_DROP_M) {
        if (!alt_dropping) {
            alt_dropping = true;
            alt_drop_start_ms = now;
        } else if (now - alt_drop_start_ms >= APOGEE_CONFIRM_MS) {
            apogee_triggered = true;
            return;
        }
    } else {
        alt_dropping = false;
    }

    // Criterion 2: freefall -- very low acceleration magnitude
    if (accel_magnitude < FREEFALL_THRESHOLD_MS2) {
        if (!in_freefall) {
            in_freefall = true;
            freefall_start_ms = now;
        } else if (now - freefall_start_ms >= FREEFALL_CONFIRM_MS) {
            apogee_triggered = true;
            return;
        }
    } else {
        in_freefall = false;
    }
}

float altimeter_get_altitude() { return altitude; }
float altimeter_get_vspeed()   { return vspeed; }
float altimeter_get_max_alt()  { return max_alt; }

bool altimeter_apogee_detected() {
    return apogee_triggered;
}

void altimeter_reset() {
    baseline_pa = 101325.0f;
    altitude = 0.0f;
    prev_altitude = 0.0f;
    vspeed = 0.0f;
    max_alt = 0.0f;
    apogee_triggered = false;
    alt_dropping = false;
    in_freefall = false;
    alt_drop_start_ms = 0;
    freefall_start_ms = 0;
}
