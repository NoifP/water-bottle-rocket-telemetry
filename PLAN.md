# Water Rocket Telemetry -- Implementation Plan

## Context

Two ESP32-based devices need firmware: a rocket (Wemos C3 Pico) that reads sensors, detects apogee, deploys a parachute, and transmits telemetry; and a ground station (CYD) that displays telemetry, logs to SD, and sends commands. Communication is via ESP-NOW at 50 Hz. Both use Arduino framework via PlatformIO.

---

## Project Structure

```
water-rocket-telemetry/
├── rocket/                        # PlatformIO project -- Wemos C3 Pico
│   ├── platformio.ini
│   └── src/
│       ├── main.cpp               # setup/loop orchestration
│       ├── config.h               # Pins, constants, thresholds
│       ├── telemetry_packet.h     # Shared packet structs (identical copy in ground/)
│       ├── sensors.h / .cpp       # MPU-6050 + BMP388/BME280 reading
│       ├── altimeter.h / .cpp     # Altitude, vertical speed, apogee detection
│       ├── parachute.h / .cpp     # Servo control with safety guards
│       ├── comms.h / .cpp         # ESP-NOW send telemetry / receive commands
│       └── state_machine.h / .cpp # Flight state machine
│
├── ground/                        # PlatformIO project -- CYD (ESP32-2432S028R)
│   ├── platformio.ini
│   └── src/
│       ├── main.cpp               # setup/loop orchestration
│       ├── config.h               # Pins, constants, calibration
│       ├── telemetry_packet.h     # Shared packet structs (identical copy)
│       ├── comms.h / .cpp         # ESP-NOW receive telemetry / send commands
│       ├── display.h / .cpp       # TFT rendering at 10 Hz
│       ├── touch.h / .cpp         # XPT2046 input, debounce, button regions
│       ├── sdlog.h / .cpp         # Buffered non-blocking CSV logging
│       └── ui_screens.h / .cpp    # Screen layouts, confirmation dialogs
│
└── shared/
    └── telemetry_packet.h         # Canonical version, manually copied to both projects
```

---

## Part 1: Rocket Firmware (Wemos C3 Pico)

### Pin Assignments

| Function       | Pin    | Notes                              |
|----------------|--------|-------------------------------------|
| I2C SDA        | GPIO 8  | MPU-6050 + BMP388/BME280 share bus (LOLIN I2C connector) |
| I2C SCL        | GPIO 10 |                                                          |
| Servo PWM      | GPIO 6 | LEDC peripheral via ESP32Servo     |
| Battery ADC    | GPIO 3 | ADC1 -- safe with WiFi active      |
| Onboard LED    | GPIO 7 | Status indication (blue LED)       |

### PlatformIO Config (`rocket/platformio.ini`)

```ini
[env:c3pico]
platform = espressif32
board = lolin_c3_pico
framework = arduino
monitor_speed = 115200
lib_deps =
    adafruit/Adafruit MPU6050@^2.2.6
    adafruit/Adafruit BMP3XX Library@^2.1.4
    adafruit/Adafruit BME280 Library@^2.2.4
    adafruit/Adafruit Unified Sensor@^1.1.14
    madhephaestus/ESP32Servo@^3.0.5
build_flags =
    -DARDUINO_USB_CDC_ON_BOOT=1
```

### Shared Packet Structures (`telemetry_packet.h`)

```cpp
enum class FlightState : uint8_t {
    IDLE=0, ARMED=1, BOOST=2, COAST=3, APOGEE=4, DESCENT=5, LANDED=6
};

enum class Command : uint8_t {
    NONE=0, ARM=1, DISARM=2, MANUAL_DEPLOY=3, PING=4
};

// Rocket -> Ground (67 bytes, well under 250-byte ESP-NOW limit)
struct __attribute__((packed)) TelemetryPacket {
    uint8_t  magic;            // 0xAA
    uint8_t  version;          // 1
    uint16_t seq;
    uint32_t timestamp_ms;
    float    pressure_pa, altitude_m, vertical_speed;
    float    accel_x, accel_y, accel_z;
    float    gyro_x, gyro_y, gyro_z;
    float    accel_magnitude;
    float    temperature_c;    // from BMP388/BME280
    float    humidity_pct;     // from BME280 (0 if using BMP388)
    FlightState state;
    uint8_t  flags;            // bit0: chute deployed, bit1: apogee, bit2: manual, bit3: low batt
    float    max_altitude_m, battery_v;
    uint8_t  checksum;         // XOR of preceding bytes
};

// Ground -> Rocket (5 bytes)
struct __attribute__((packed)) CommandPacket {
    uint8_t magic;             // 0xBB
    Command command;
    uint16_t token;            // challenge token for safety
    uint8_t checksum;
};
```

### Flight State Machine

```
IDLE ──[ARM cmd]──> ARMED ──[accel > 3g for 60ms]──> BOOST
  ^                   |                                  |
  | DISARM/timeout    |                    [accel < 1.5g]|
  +-------------------+                                  v
                                                       COAST
                                                         |
                                    [alt dropping 0.5m for 100ms
                                     OR freefall for 80ms]
                                                         v
                                                       APOGEE -> deploy chute
                                                         |
                                                         v
                                                      DESCENT
                                                         |
                                      [vspeed ~0, alt < 2m for 3s]
                                                         v
                                                       LANDED -> auto-reset to IDLE
```

**Manual deploy** accepted in any state except IDLE (the "oh no" button).

### Safety Rules

1. Servo pin detached (no PWM) in IDLE -- cannot fire accidentally
2. ARM requires explicit ground station command + echoed challenge token
3. DISARM ignored during active flight (BOOST through DESCENT)
4. Deploy is one-shot; subsequent calls are no-ops
5. ARMED auto-disarms after 15 minutes with no launch detected

### Main Loop (50 Hz)

1. Read MPU-6050 + BMP388 via I2C (~1.2ms)
2. Update altitude filter, apogee detection
3. Check for incoming ESP-NOW commands
4. Update state machine (transition checks, parachute deploy)
5. Build and send telemetry packet (~1-2ms)
6. Update status LED

**Total loop time: ~3-4ms of 20ms budget.** In IDLE with no commands for 10s, drops to 2 Hz to save power.

### LED Patterns

| State   | Pattern                    |
|---------|----------------------------|
| IDLE    | Slow blink (1s on/off)     |
| ARMED   | Fast blink (200ms on/off)  |
| BOOST   | Solid ON                   |
| APOGEE  | Triple flash               |
| DESCENT | Double flash every 500ms   |
| LANDED  | Solid ON                   |
| Error   | Rapid strobe (50ms)        |

---

## Part 2: Ground Station (CYD -- ESP32-2432S028R v3)

### PlatformIO Config (`ground/platformio.ini`)

```ini
[env:cyd]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
lib_deps =
    bodmer/TFT_eSPI@^2.5.43
    paulstoffregen/XPT2046_Touchscreen@^1.4
build_flags =
    -DUSER_SETUP_LOADED=1
    -DST7789_DRIVER=1
    -DTFT_WIDTH=240
    -DTFT_HEIGHT=320
    -DTFT_MISO=12 -DTFT_MOSI=13 -DTFT_SCLK=14 -DTFT_CS=15 -DTFT_DC=2
    -DTFT_RST=-1 -DTFT_BL=21 -DTFT_BACKLIGHT_ON=HIGH
    -DTFT_INVERSION_ON=1
    -DSPI_FREQUENCY=55000000
    -DUSE_HSPI_PORT=1
    -DLOAD_GLCD=1 -DLOAD_FONT2=1 -DLOAD_FONT4=1
```

Key: **ST7789_DRIVER** + **TFT_INVERSION_ON** for CYD v3. Touch uses XPT2046_Touchscreen (not TFT_eSPI built-in touch). SD card uses default VSPI pins (CS=5).

### Screen Layout (240x320 portrait)

```
+------------------------------------------+
|  ROCKET TELEMETRY        [####-]  -45dBm |  Header + signal
|------------------------------------------|
|  ALT      12.3 m                         |
|  SPD       5.7 m/s                       |
|  ACC       2.1 g                         |
|  TIME     03.42 s                        |
|  STATE   [ BOOST ]                       |  Color-coded badge
|------------------------------------------|
|  [  ARM / DISARM  ]   [ DEPLOY CHUTE ]   |  Touch buttons
|  SD: OK  1,204 rows     PKT: 48/50 Hz   |  Status footer
+------------------------------------------+
```

- Values rendered in large Font4, labels in smaller Font2 (cyan)
- DEPLOY button greyed out unless armed; requires confirmation dialog with 5s auto-dismiss
- State badge color: IDLE=grey, ARMED=yellow, BOOST/COAST=orange, APOGEE=green, DESCENT=cyan, LANDED=white

### Main Loop Architecture

```
loop() runs freely, subsystems rate-limited:
  - ESP-NOW receive:  handled by callback -> volatile buffer (async)
  - Touch polling:    20 Hz (every 50ms)
  - Display update:   10 Hz (every 100ms)
  - SD flush:         write a few buffered rows each iteration
  - Signal quality:   1 Hz recalculation
```

### SD Card Logging

- **Ring buffer** of 128 packets in RAM (~5 KB) absorbs SD write latency spikes
- CSV format: `ground_ms,rocket_ms,altitude_m,speed_mps,accel_g,accel_x,accel_y,accel_z,pressure_pa,temp_c,humidity_pct,state,chute,batt_v,rssi`
- File naming: `/flight_0001.csv`, auto-incrementing
- New file created on ARM; flushed and closed on LANDED
- `file.flush()` every 1-2 seconds (file stays open)

### Touch Handling

- XPT2046 on software SPI (CLK=25, CS=33, IRQ=36)
- Raw ADC (0-4095) mapped to screen coordinates with calibration constants
- 50ms debounce, 300ms minimum between activations
- Button hit detection via simple rectangle containment

### Error Handling

- **No SD**: show "NO SD" in footer, continue without logging
- **No signal**: show "WAITING..." or "SIGNAL LOST" after 2s timeout, display "--" for all values
- **Deploy safety**: button disabled unless armed, confirmation dialog required

### RGB LED (CYD pins 4, 16, 17, active LOW)

- Green: connected, receiving telemetry
- Yellow: armed
- Red blinking: signal lost
- Blue flash: command sent

---

## ESP-NOW Compatibility Note

ESP32-C3 (rocket) and original ESP32 (ground station) are confirmed compatible over ESP-NOW. Both must use the same WiFi channel (default: 1). MAC addresses hardcoded in each device's `config.h`.

---

## Implementation Order

1. **Rocket sensor bring-up**: PlatformIO project, I2C init, MPU-6050 + BMP388 reading, Serial output
2. **Ground station display bring-up**: PlatformIO project, TFT hello world, touch calibration, SD test
3. **ESP-NOW link**: Rocket sends dummy packets, ground station receives and prints
4. **Rocket telemetry pipeline**: Altitude computation, vertical speed, full packet assembly
5. **Ground station display**: Telemetry rendering, signal quality, touch buttons
6. **SD logging**: Ring buffer, CSV writes, file management
7. **Rocket state machine**: All transitions, apogee detection, parachute servo
8. **Command system**: ARM/DISARM/DEPLOY with challenge tokens
9. **Integration testing**: End-to-end with both boards
10. **Hardening**: Power management, LED patterns, watchdog, threshold tuning

## Verification

- Bench test rocket by shaking (boost detection), lifting (altitude), dropping (apogee)
- Verify ESP-NOW link at increasing distances
- Confirm SD logging survives sustained 50 Hz writes for 60+ seconds
- Test manual deploy confirmation flow on touchscreen
- Verify servo safety: power cycle in every state, confirm it never fires in IDLE
