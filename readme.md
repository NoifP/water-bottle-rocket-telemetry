# Water Bottle Rocket Telemetry

**AI Warning: this whole thing was built by Claude. Nothing has been tested (CYDs are in the post)**

This is software for an esp32 with some sensors and a servo (for parachute release) on a water bottle rocket. The esp32 on the rocket should feed back telemetry to the esp32 ground station (a Cheap Yellow Display / CYD) via ESP-NOW or something else if more appropriate.

The ground station will show altitude, speed, acceleration, flight time and a button for manual parachute release.

The ground station will record all received telemetry to a microsd card.

## Hardware

### Rocket

* Brains: Wemos S2 Mini - ESP32-S2FN4R2
* Gyro: MPU-6050
* Pressure / Altitude: BMP388 or BME280
* Parachute release: 9g servo
* Battery: LoPo

### Ground Station

* Brains + Screen: ESP32-2432S028R ( these boards have both usb-c and micro-usb power connectors, so are the 'version 3' variant of the board with the st7789 display driver )



## Next steps

1. Update GROUND_MAC / ROCKET_MAC in each config.h with actual MAC addresses (printed to Serial on boot)
1. Calibrate touch constants with your actual CYD hardware
1. Tune apogee detection thresholds with bench testing (shake/drop the board)


