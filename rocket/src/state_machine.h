#pragma once

#include "telemetry_packet.h"
#include "sensors.h"

// Initialize the state machine to IDLE
void state_machine_init();

// Update state machine. Call once per loop iteration.
void state_machine_update(
    const SensorData& sensors,
    float altitude,
    float vspeed,
    bool apogee_detected,
    const CommandPacket* cmd  // nullptr if no command this tick
);

// Get current flight state
FlightState state_machine_get_state();

// Get flags byte for telemetry
uint8_t state_machine_get_flags();
