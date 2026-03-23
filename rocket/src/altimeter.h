#pragma once

// Initialize altimeter module
void altimeter_init();

// Set baseline pressure (call on ARM, averages multiple readings)
void altimeter_set_baseline(float pressure_pa);

// Update with new pressure reading. dt = time since last call in seconds.
void altimeter_update(float pressure_pa, float accel_magnitude, float dt);

// Getters
float altimeter_get_altitude();     // metres AGL
float altimeter_get_vspeed();       // m/s, positive = ascending
float altimeter_get_max_alt();      // peak altitude so far

// Apogee detection -- returns true once when apogee criteria met
bool altimeter_apogee_detected();

// Reset state (call on disarm / return to idle)
void altimeter_reset();
