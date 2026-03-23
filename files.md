# File Reference

A description of every source file in the project, organised by folder.

---

## `shared/`

The canonical version of files shared between both projects. These are manually copied into `rocket/src/` and `ground/src/` -- always edit the `shared/` version first, then copy it to both.

| File | Purpose |
|------|---------|
| `telemetry_packet.h` | Defines the communication protocol between rocket and ground station. Contains the `TelemetryPacket` struct (rocket-to-ground, 67 bytes), the `CommandPacket` struct (ground-to-rocket, 5 bytes), the `FlightState` and `Command` enums, flag bit definitions, an XOR checksum function, and a state-to-string helper. Both devices must use an identical copy of this file. |

---

## `rocket/`

PlatformIO project for the Wemos S2 Mini (ESP32-S2) that flies on the rocket.

| File | Purpose |
|------|---------|
| `platformio.ini` | PlatformIO build configuration. Selects the `lolin_s2_mini` board, declares library dependencies (Adafruit MPU6050, BMP3XX, BME280, ESP32Servo), and sets the `ARDUINO_USB_CDC_ON_BOOT` flag required for serial output over the S2 Mini's native USB. |

### `rocket/src/`

| File | Purpose |
|------|---------|
| `main.cpp` | Entry point. Runs `setup()` to initialise all modules, then a `loop()` that executes at 50 Hz: reads sensors, updates the altimeter, checks for incoming commands, runs the state machine, builds a telemetry packet, transmits it via ESP-NOW, and blinks the status LED. Drops to 2 Hz in idle to save power. |
| `config.h` | Central configuration. All GPIO pin assignments (SDA, SCL, servo, battery ADC, LED), timing intervals (loop rate, idle timeout), flight detection thresholds (boost acceleration, apogee altitude drop, freefall g-threshold, landing speed), servo angles (locked/deployed), battery voltage divider ratio, and the ground station MAC address. Change this file to tune behaviour -- no other file contains magic numbers. |
| `telemetry_packet.h` | Copy of `shared/telemetry_packet.h`. Must be kept identical to the ground station's copy. |
| `sensors.h` | Header for the sensor module. Declares `SensorData` (accel, gyro, pressure, temperature, humidity) and the `sensors_init()` / `sensors_read()` interface. |
| `sensors.cpp` | Initialises the I2C bus at 400 kHz, probes for the MPU-6050 (accelerometer/gyro at 0x68), then tries a BMP388 (barometer at 0x77) and falls back to a BME280 (at 0x76) if not found. Configures sensor ranges and filter bandwidths. `sensors_read()` populates a `SensorData` struct with the latest readings from both sensors in a single call. |
| `altimeter.h` | Header for the altitude module. Declares functions to set the pressure baseline, update altitude, get vertical speed, track peak altitude, and detect apogee. |
| `altimeter.cpp` | Converts raw pressure to altitude AGL using the barometric formula, with the baseline captured at arming time. Applies exponential smoothing to altitude and vertical speed. Detects apogee using two independent criteria: (1) barometric -- altitude has dropped more than 0.5 m from the peak for 100 ms, or (2) freefall -- acceleration magnitude below 2 m/s^2 for 80 ms. Either criterion triggers apogee. |
| `parachute.h` | Header for the parachute servo module. Declares arm, deploy, disarm, and reset functions. |
| `parachute.cpp` | Controls a 9g servo via the ESP32 LEDC peripheral (ESP32Servo library). On init, briefly moves the servo to the locked angle then detaches it (no PWM output) so it cannot fire accidentally. `parachute_deploy()` refuses to actuate if the flight state is IDLE. After deploying, holds the angle for 1 second then detaches to save power. Deploy is one-shot -- subsequent calls are no-ops. |
| `comms.h` | Header for the communications module. Declares functions to send telemetry, receive commands, manage challenge tokens, and print the device MAC. |
| `comms.cpp` | Initialises WiFi in station mode (no AP connection) and sets up ESP-NOW. Registers the ground station as a peer. The receive callback validates incoming `CommandPacket`s (magic byte + checksum) and stores them in a volatile buffer for the main loop to consume. Generates random challenge tokens on arming for command authentication. |
| `state_machine.h` | Header for the flight state machine. Declares `state_machine_update()` and getters for current state and flags. |
| `state_machine.cpp` | Implements the flight state machine with seven states: IDLE, ARMED, BOOST, COAST, APOGEE, DESCENT, LANDED. Evaluates transition conditions each tick: ARM command enters ARMED, sustained high-g enters BOOST, thrust drop enters COAST, apogee detection triggers chute deploy and enters DESCENT, low speed near ground level enters LANDED. Handles manual deploy override in any active flight state. Enforces safety rules: DISARM ignored during active flight, 15-minute armed timeout, challenge token validation on critical commands. |

---

## `ground/`

PlatformIO project for the ESP32-2432S028R (Cheap Yellow Display) that serves as the ground station.

| File | Purpose |
|------|---------|
| `platformio.ini` | PlatformIO build configuration. Selects the `esp32dev` board, declares TFT_eSPI and XPT2046_Touchscreen library dependencies, and passes all TFT driver settings as build flags: ST7789 driver, `TFT_INVERSION_ON` (required for CYD v3 colour correctness), SPI pin assignments, font loading, and 55 MHz SPI clock. |

### `ground/src/`

| File | Purpose |
|------|---------|
| `main.cpp` | Entry point. Runs `setup()` to initialise the display, touch, ESP-NOW, and SD card, then a free-running `loop()` with rate-limited subsystems: checks for new telemetry packets (async via callback), polls touch at 20 Hz, updates the display at 10 Hz, flushes SD writes incrementally, recalculates signal quality at 1 Hz, and drives the RGB LED. Handles the ARM/DISARM and DEPLOY button logic including the confirmation dialog with a 5-second auto-dismiss timeout. |
| `config.h` | Central configuration. Touch calibration constants (raw ADC range, axis swap/invert), SD card chip select pin, RGB LED pins, display/touch/signal update intervals, SD ring buffer size, the rocket MAC address, and screen dimensions. |
| `telemetry_packet.h` | Copy of `shared/telemetry_packet.h`. Must be kept identical to the rocket's copy. |
| `comms.h` | Header for the communications module. Declares functions to receive telemetry, send commands, get RSSI, and print the device MAC. |
| `comms.cpp` | Initialises WiFi in station mode and sets up ESP-NOW. The receive callback validates incoming `TelemetryPacket`s (magic byte + checksum) and stores them in a volatile buffer. Provides `comms_send_command()` to send ARM, DISARM, or MANUAL_DEPLOY commands to the rocket with a challenge token and checksum. |
| `display.h` | Header for the display module. Declares `AppState` (the central data struct holding latest telemetry, signal quality, flight timing, SD status, and UI state) and functions to draw the layout, update values, and manage the confirmation dialog. |
| `display.cpp` | Renders the ground station UI on the 240x320 TFT using TFT_eSPI. Draws a header bar with signal-strength bars, five telemetry fields (altitude, speed, acceleration, flight time, state) with field-level clearing to avoid flicker, a colour-coded flight state badge, ARM/DEPLOY buttons, and a status footer with SD and packet rate info. Handles the "no signal" and "waiting" states. Draws and clears the modal deploy confirmation dialog. |
| `touch.h` | Header for the touch module. Declares button IDs (ARM, DEPLOY, CONFIRM_YES, CONFIRM_NO) and functions to poll, enable/disable buttons, and toggle confirmation mode. |
| `touch.cpp` | Initialises the XPT2046 touchscreen on a separate SPI bus. Maps raw ADC coordinates (0--4095) to screen pixels using calibration constants from `config.h`. Implements debouncing (300 ms cooldown between presses) and button hit-testing against predefined screen rectangles. Supports two modes: normal (ARM + DEPLOY buttons) and confirmation (YES + NO buttons). |
| `sdlog.h` | Header for the SD logging module. Declares functions to start/end a flight log, enqueue packets, and flush to disk. |
| `sdlog.cpp` | Manages non-blocking CSV logging to the microSD card. Uses a 128-entry ring buffer in RAM to absorb SD write latency spikes. On ARM, creates a new file (`flight_0001.csv`, auto-incrementing) with a CSV header. `sdlog_enqueue()` pushes packets into the ring buffer. `sdlog_flush_some()` writes up to 10 rows per call and calls `file.flush()` every 2 seconds. On LANDED, drains the buffer and closes the file. Handles buffer-full gracefully by dropping the oldest entry. |
