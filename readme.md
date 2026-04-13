# Water Bottle Rocket Telemetry

**AI Warning: this whole thing was built by Claude. Nothing has been well tested **

This is software for an esp32 with some sensors and a servo (for parachute release) on a water bottle rocket. The esp32 on the rocket should feed back telemetry to the esp32 ground station (a Cheap Yellow Display / CYD) via ESP-NOW or something else if more appropriate.

The ground station will show altitude, speed, acceleration, flight time and a button for manual parachute release.

The ground station will record all received telemetry to a microsd card.

## Hardware

### Rocket

* Brains: Wemos C3 Pico - ESP32-C3 (chosen for built-in LiPo battery charging controller — no separate charging circuit needed)
* Gyro: MPU-6050
* Pressure / Altitude: BMP388 or BME280
* Parachute release: 9g servo
* Battery: Tiny LiPo

### Ground Station

* Brains + Screen: ESP32-2432S028R ( these boards have both usb-c and micro-usb power connectors, so are the 'version 3' variant of the board with the st7789 display driver )

## Ground station touch calibration

The touchscreen ships with default calibration values that may be off for your specific CYD panel. To run the on-device calibration wizard:

1. Power off the ground station.
2. Press and hold your finger on the screen.
3. Power on while keeping your finger held on the screen.
4. A progress bar appears — hold for 2 seconds until it fills. Release early to skip.
5. Tap the crosshair in the **top-left** corner when prompted (1 of 2).
6. Tap the crosshair in the **bottom-right** corner when prompted (2 of 2).
7. "Calibration saved!" is shown and the normal UI resumes.

Calibration is stored in NVS flash and survives reboots. To recalibrate, repeat from step 1.

The footer of the main screen shows `T:x,y  R:x,y` — mapped screen coordinates and raw ADC values — useful for verifying calibration without serial output.

## Next steps

1. Update GROUND_MAC / ROCKET_MAC in each config.h with actual MAC addresses (printed to Serial on boot)
1. Run touch calibration on your CYD hardware (see above)
1. Tune apogee detection thresholds with bench testing (shake/drop the board)


