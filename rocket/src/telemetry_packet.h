#pragma once
#include <stdint.h>
#include <stddef.h>

// ============================================================================
// Shared telemetry protocol between rocket and ground station
// This file must be identical in rocket/src/ and ground/src/
// Canonical version lives in shared/
// ============================================================================

#define TELEMETRY_MAGIC  0xAA
#define COMMAND_MAGIC    0xBB
#define PROTOCOL_VERSION 2

// --- Flight states ---
enum class FlightState : uint8_t {
    IDLE    = 0,
    ARMED   = 1,
    BOOST   = 2,
    COAST   = 3,
    APOGEE  = 4,
    DESCENT = 5,
    LANDED  = 6,
};

// --- Commands (ground -> rocket) ---
enum class Command : uint8_t {
    NONE            = 0,
    ARM             = 1,
    DISARM          = 2,
    MANUAL_DEPLOY   = 3,
    PING            = 4,
    DISCOVERY_REPLY = 5, // Ground acknowledges a FLAG_DISCOVERY telemetry packet
};

// --- Telemetry flags bitfield ---
#define FLAG_CHUTE_DEPLOYED  (1 << 0)
#define FLAG_APOGEE_DETECTED (1 << 1)
#define FLAG_MANUAL_DEPLOY   (1 << 2)
#define FLAG_LOW_BATTERY     (1 << 3)
#define FLAG_DISCOVERY       (1 << 4) // Rocket is probing for the ground station's channel

// --- Rocket -> Ground telemetry packet (67 bytes) ---
struct __attribute__((packed)) TelemetryPacket {
    uint8_t     magic;           // TELEMETRY_MAGIC (0xAA)
    uint8_t     version;         // PROTOCOL_VERSION
    uint16_t    seq;             // Sequence number (wraps at 65535)
    uint32_t    timestamp_ms;    // millis() on rocket

    // Barometric
    float       pressure_pa;     // Raw pressure in Pascals
    float       altitude_m;      // Filtered altitude AGL (metres, zeroed at arm)
    float       vertical_speed;  // m/s (positive = ascending)

    // IMU
    float       accel_x;         // m/s^2, body frame
    float       accel_y;
    float       accel_z;
    float       gyro_x;          // deg/s
    float       gyro_y;
    float       gyro_z;
    float       accel_magnitude; // sqrt(x^2+y^2+z^2)

    // Environment
    float       temperature_c;   // From BMP388/BME280
    float       humidity_pct;    // From BME280 (0 if using BMP388)

    // State
    FlightState state;
    uint8_t     flags;           // FLAG_* bitfield
    float       max_altitude_m;  // Peak altitude so far
    float       battery_v;       // LiPo voltage

    // Integrity
    uint8_t     checksum;        // XOR of all preceding bytes
};

// --- Ground -> Rocket command packet (5 bytes) ---
struct __attribute__((packed)) CommandPacket {
    uint8_t  magic;      // COMMAND_MAGIC (0xBB)
    Command  command;
    uint16_t token;      // Challenge token echoed from rocket for safety
    uint8_t  checksum;   // XOR of all preceding bytes
};

// --- Utility: compute XOR checksum over a buffer ---
inline uint8_t compute_checksum(const void* data, size_t len) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    uint8_t chk = 0;
    for (size_t i = 0; i < len; i++) {
        chk ^= p[i];
    }
    return chk;
}

// --- Utility: flight state to string ---
inline const char* flight_state_str(FlightState s) {
    switch (s) {
        case FlightState::IDLE:    return "IDLE";
        case FlightState::ARMED:   return "ARMED";
        case FlightState::BOOST:   return "BOOST";
        case FlightState::COAST:   return "COAST";
        case FlightState::APOGEE:  return "APOGEE";
        case FlightState::DESCENT: return "DESCENT";
        case FlightState::LANDED:  return "LANDED";
        default:                   return "???";
    }
}
