#pragma once

#include "telemetry_packet.h"

// Initialize servo to locked position, then detach
void parachute_init();

// Attach servo and confirm locked position (call on ARM)
void parachute_arm();

// Deploy parachute (move servo to deploy angle). Only works if state != IDLE.
// Returns true if deploy was executed, false if blocked by safety.
bool parachute_deploy(FlightState current_state);

// Return to locked position and detach (call on DISARM)
void parachute_disarm();

// Has the parachute been deployed this flight?
bool parachute_is_deployed();

// Reset deployed flag (call on return to IDLE)
void parachute_reset();
