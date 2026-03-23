#include "sensors.h"
#include "config.h"
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_BMP3XX.h>
#include <Adafruit_BME280.h>
#include <math.h>

static Adafruit_MPU6050 mpu;
static Adafruit_BMP3XX bmp;
static Adafruit_BME280 bme;

static bool mpu_found = false;
static bool bmp388_found = false;
static bool bme280_found = false;

bool sensors_init() {
    Wire.begin(PIN_SDA, PIN_SCL);
    Wire.setClock(400000); // 400 kHz I2C

    // --- MPU-6050 ---
    if (mpu.begin(0x68, &Wire)) {
        mpu_found = true;
        mpu.setAccelerometerRange(MPU6050_RANGE_16_G);
        mpu.setGyroRange(MPU6050_RANGE_500_DEG);
        mpu.setFilterBandwidth(MPU6050_BAND_44_HZ);
        Serial.println("[sensors] MPU-6050 OK");
    } else {
        Serial.println("[sensors] MPU-6050 NOT FOUND");
    }

    // --- Try BMP388 first (address 0x77) ---
    if (bmp.begin_I2C(0x77, &Wire)) {
        bmp388_found = true;
        bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_2X);
        bmp.setPressureOversampling(BMP3_OVERSAMPLING_8X);
        bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
        bmp.setOutputDataRate(BMP3_ODR_50_HZ);
        Serial.println("[sensors] BMP388 OK");
    } else {
        Serial.println("[sensors] BMP388 not found, trying BME280...");
        // --- Fall back to BME280 (address 0x76) ---
        if (bme.begin(0x76, &Wire)) {
            bme280_found = true;
            bme.setSampling(
                Adafruit_BME280::MODE_NORMAL,
                Adafruit_BME280::SAMPLING_X1,  // temperature
                Adafruit_BME280::SAMPLING_X8,  // pressure
                Adafruit_BME280::SAMPLING_X1,  // humidity
                Adafruit_BME280::FILTER_X4,
                Adafruit_BME280::STANDBY_MS_20 // ~50 Hz
            );
            Serial.println("[sensors] BME280 OK");
        } else {
            Serial.println("[sensors] BME280 NOT FOUND -- no barometer!");
        }
    }

    return bmp388_found || bme280_found;
}

bool sensors_read(SensorData& data) {
    data.imu_ok = false;
    data.baro_ok = false;
    data.humidity_pct = 0.0f;

    // --- Read MPU-6050 ---
    if (mpu_found) {
        sensors_event_t a, g, temp;
        if (mpu.getEvent(&a, &g, &temp)) {
            data.accel_x = a.acceleration.x;
            data.accel_y = a.acceleration.y;
            data.accel_z = a.acceleration.z;
            data.gyro_x = g.gyro.x * 57.2958f; // rad/s -> deg/s
            data.gyro_y = g.gyro.y * 57.2958f;
            data.gyro_z = g.gyro.z * 57.2958f;
            data.accel_magnitude = sqrtf(
                data.accel_x * data.accel_x +
                data.accel_y * data.accel_y +
                data.accel_z * data.accel_z
            );
            data.imu_ok = true;
        }
    }

    // --- Read barometer ---
    if (bmp388_found) {
        if (bmp.performReading()) {
            data.pressure_pa = bmp.pressure;
            data.temperature_c = bmp.temperature;
            data.baro_ok = true;
        }
    } else if (bme280_found) {
        data.pressure_pa = bme.readPressure();
        data.temperature_c = bme.readTemperature();
        data.humidity_pct = bme.readHumidity();
        data.baro_ok = (data.pressure_pa > 0);
    }

    return data.imu_ok && data.baro_ok;
}

bool sensors_has_humidity() {
    return bme280_found;
}
