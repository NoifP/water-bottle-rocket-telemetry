# Setup Guide -- Water Rocket Telemetry

This guide covers wiring the sensors and servo to the rocket board, preparing the ground station, and flashing both devices.

---

## Prerequisites

- **PlatformIO** installed (either the VS Code extension or CLI)
- USB-C cable for the Wemos S2 Mini
- USB-C or micro-USB cable for the CYD ground station
- A microSD card (FAT32 formatted) for flight logging

---

## Part 1: Rocket Wiring (Wemos S2 Mini)

The rocket has three external connections: two I2C sensors and one servo. All share the same power rails.

### Wemos S2 Mini Pinout Reference

```
        (USB end down, top view)

  Left side          Right side
  ---------          ----------
  EN    1            40    39
  3     2            38    37
  5     4            36    35  <-- SCL
  7     6            34    33  <-- SDA
  9     8            21    18
  11    10           17    16
  12    13           GND   GND
  3.3V  14           15    VBUS
                    (near USB)
```

### I2C Bus (shared by both sensors)

Both the MPU-6050 and the BMP388 (or BME280) connect to the same I2C bus:

| Signal | S2 Mini Pin | Wire Colour (suggested) |
|--------|-------------|------------------------|
| SDA    | GPIO 33     | Blue                   |
| SCL    | GPIO 35     | Yellow                 |
| VCC    | 3.3V        | Red                    |
| GND    | GND         | Black                  |

The sensors have different I2C addresses so there are no conflicts:

| Sensor  | I2C Address | Provides                     |
|---------|-------------|------------------------------|
| MPU-6050 | 0x68       | Accelerometer + Gyroscope    |
| BMP388   | 0x77       | Pressure + Temperature       |
| BME280   | 0x76       | Pressure + Temperature + Humidity |

Use **either** a BMP388 or a BME280 for the barometer -- the firmware auto-detects which is present. If you want humidity data in your logs, use the BME280.

### Wiring Diagram -- I2C Sensors

```
S2 Mini                    MPU-6050          BMP388 / BME280
--------                   --------          ---------------
3.3V  ──────┬───────────── VCC               VCC ──────┘
            │                                           │
GND   ──────┼───────────── GND               GND ──────┘
            │                                           │
GPIO 33 ────┼───────────── SDA               SDA ──────┘
(SDA)       │                                           │
GPIO 35 ────┴───────────── SCL               SCL ──────┘
(SCL)
```

Both sensors connect in parallel on the same 4 wires. If your sensor breakout boards have pull-up resistors (most do), no external pull-ups are needed.

### Parachute Servo

| Signal | S2 Mini Pin | Servo Wire |
|--------|-------------|------------|
| PWM    | GPIO 11     | Signal (orange/white) |
| VCC    | VBUS (5V)   | Power (red) |
| GND    | GND         | Ground (brown/black) |

**Important:** Power the servo from **VBUS** (5V USB rail), not 3.3V. The 3.3V regulator cannot supply enough current for a servo. When running on battery, ensure your LiPo feeds through VBUS or use a separate 5V regulator for the servo.

### Battery Voltage Monitoring (optional)

To monitor the LiPo voltage, connect a voltage divider to **GPIO 3**:

```
LiPo (+) ──── [R1 10kΩ] ──┬── GPIO 3 (ADC)
                            │
                      [R2 10kΩ]
                            │
GND ────────────────────────┘
```

With equal resistors, the divider ratio is 2:1, matching the `BATTERY_DIVIDER_RATIO` constant in `rocket/src/config.h`. Adjust the resistor values and the constant if using a different ratio.

### Summary -- All Rocket Connections

| S2 Mini Pin | Connected To         |
|-------------|----------------------|
| GPIO 33     | SDA (MPU-6050 + BMP388/BME280) |
| GPIO 35     | SCL (MPU-6050 + BMP388/BME280) |
| GPIO 11     | Servo signal wire    |
| GPIO 3      | Battery voltage divider (optional) |
| GPIO 15     | Onboard LED (built-in, no wiring needed) |
| 3.3V        | Sensor VCC           |
| VBUS (5V)   | Servo VCC            |
| GND         | Common ground        |

---

## Part 2: Ground Station (CYD -- ESP32-2432S028R)

The CYD is a self-contained board with a built-in 2.8" TFT display, touchscreen, SD card slot, and RGB LED. **No external wiring is needed** -- just plug in a USB cable and insert a microSD card.

### Preparing the SD Card

1. Use a microSD card (any size, even 1 GB is plenty)
2. Format it as **FAT32**
3. Insert it into the TF (microSD) slot on the back of the CYD board
4. Flight logs will be saved as `flight_0001.csv`, `flight_0002.csv`, etc.

### CYD Board Features (for reference)

The firmware uses these built-in CYD peripherals:

| Peripheral       | CYD Pins (pre-wired)       | Notes |
|------------------|---------------------------|-------|
| TFT Display      | HSPI (MOSI=13, SCLK=14, CS=15, DC=2, BL=21) | ST7789 driver, 240x320 |
| Touchscreen      | Software SPI (CLK=25, MOSI=32, CS=33, IRQ=36, MISO=39) | XPT2046 |
| SD Card          | VSPI (SCK=18, MISO=19, MOSI=23, CS=5) | FAT32 microSD |
| RGB LED          | R=4, G=16, B=17           | Active LOW |

All of these are on-board -- you do not need to connect any wires.

---

## Part 3: Flashing the Firmware

### Step 1: Install PlatformIO

If you don't have PlatformIO yet:

- **VS Code**: Install the "PlatformIO IDE" extension from the marketplace
- **CLI**: `pip install platformio`

### Step 2: Flash the Rocket (Wemos S2 Mini)

1. Connect the S2 Mini to your computer via USB-C

2. **Enter bootloader mode** (required for first flash on S2 Mini):
   - Hold the **0** button (GPIO 0, near the USB port)
   - While holding, press and release the **RST** button
   - Release the **0** button
   - The board should now appear as a USB device

3. Open a terminal in the project root and run:
   ```
   cd rocket
   pio run --target upload
   ```

4. After the first flash, subsequent uploads usually work without manually entering bootloader mode

5. Open the serial monitor to verify:
   ```
   pio device monitor
   ```
   You should see:
   ```
   === Water Rocket Telemetry - Rocket ===
   [sensors] MPU-6050 OK
   [sensors] BMP388 OK
   [comms] ESP-NOW initialized
   [comms] Rocket MAC: XX:XX:XX:XX:XX:XX    <-- note this!
   [parachute] Init -- locked and detached
   Setup complete. Entering main loop.
   ```

6. **Write down the Rocket MAC address** displayed in the serial output -- you'll need it for the ground station config.

### Step 3: Flash the Ground Station (CYD)

1. Connect the CYD to your computer via USB-C (the USB-C port, not the micro-USB)

2. Open a terminal in the project root and run:
   ```
   cd ground
   pio run --target upload
   ```

3. Open the serial monitor:
   ```
   pio device monitor
   ```
   You should see:
   ```
   === Water Rocket Telemetry - Ground Station ===
   [display] TFT initialized
   [touch] Initialized
   [comms] ESP-NOW initialized
   [comms] Ground MAC: YY:YY:YY:YY:YY:YY    <-- note this!
   [sdlog] SD card OK. XX MB total, XX MB used
   Setup complete. Waiting for telemetry...
   ```

4. **Write down the Ground Station MAC address** displayed in the serial output.

### Step 4: Configure MAC Addresses

By default, both devices use broadcast (FF:FF:FF:FF:FF:FF), which works for basic testing. For reliable operation, configure each device with the other's MAC address:

1. Edit `rocket/src/config.h` -- replace the `GROUND_MAC` line:
   ```cpp
   static const uint8_t GROUND_MAC[] = {0xYY, 0xYY, 0xYY, 0xYY, 0xYY, 0xYY};
   ```

2. Edit `ground/src/config.h` -- replace the `ROCKET_MAC` line:
   ```cpp
   static const uint8_t ROCKET_MAC[] = {0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX};
   ```

3. Re-flash both devices:
   ```
   cd rocket && pio run --target upload
   cd ../ground && pio run --target upload
   ```

### Step 5: Verify Communication

1. Power both devices
2. The ground station should show "WAITING..." initially
3. Within a few seconds, telemetry values should appear on the CYD screen
4. The signal indicator in the top-right should show green bars
5. The RGB LED on the CYD should glow green

---

## Part 4: Touch Calibration

The touchscreen may need calibration for your specific CYD unit. If button presses don't register correctly:

1. Enable raw touch output by opening the serial monitor on the ground station
2. Touch each corner of the screen and note the raw X/Y values printed
3. Update the calibration constants in `ground/src/config.h`:
   ```cpp
   #define TOUCH_MIN_X    200   // raw value at left edge
   #define TOUCH_MAX_X    3800  // raw value at right edge
   #define TOUCH_MIN_Y    200   // raw value at top edge
   #define TOUCH_MAX_Y    3800  // raw value at bottom edge
   #define TOUCH_SWAP_XY  false // set true if X/Y axes are swapped
   #define TOUCH_INVERT_X false // set true if X is mirrored
   #define TOUCH_INVERT_Y true  // set true if Y is mirrored
   ```
4. Re-flash the ground station after adjusting

---

## Troubleshooting

### Rocket

| Problem | Solution |
|---------|----------|
| "MPU-6050 NOT FOUND" | Check SDA/SCL wiring. Verify 3.3V power. Try swapping SDA/SCL. |
| "No barometer found" | Check I2C wiring. Try address 0x76 if BMP388 not found at 0x77. |
| Servo twitches on power-up | Normal -- it briefly moves to the locked position then detaches. |
| No serial output | Ensure `ARDUINO_USB_CDC_ON_BOOT=1` is in platformio.ini. Re-enter bootloader mode and flash again. |
| Upload fails | Enter bootloader mode manually: hold GPIO 0, press RST, release GPIO 0. |

### Ground Station

| Problem | Solution |
|---------|----------|
| White/garbled screen | Confirm your CYD is the v3 variant (ST7789). Check that `TFT_INVERSION_ON` is in the build flags. |
| Touch not responding | Use XPT2046_Touchscreen library (not TFT_eSPI touch). Check calibration constants. |
| "SD card init FAILED" | Ensure FAT32 format. Check the card is fully inserted. Try a different card. |
| "WAITING..." won't clear | Verify both devices are on the same WiFi channel (default 1). Check MAC addresses. Power cycle both. |
| Buttons register in wrong position | Recalibrate touchscreen (see Part 4 above). |
