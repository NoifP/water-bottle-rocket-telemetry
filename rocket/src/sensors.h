#pragma once
#include <stdint.h>

struct SensorData {
    float accel_x, accel_y, accel_z;  // m/s^2
    float gyro_x, gyro_y, gyro_z;    // deg/s
    float accel_magnitude;            // sqrt(x^2+y^2+z^2)
    float pressure_pa;
    float temperature_c;
    float humidity_pct;               // 0 if using BMP388 (no humidity)
    bool  imu_ok;
    bool  baro_ok;
};

// Initialize I2C and sensors. Returns true if at least baro found.
bool sensors_init();

// Read all sensors into data struct. Returns true if all reads succeeded.
bool sensors_read(SensorData& data);

// Returns true if BME280 is being used (has humidity), false for BMP388.
bool sensors_has_humidity();
