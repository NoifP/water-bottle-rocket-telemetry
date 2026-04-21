#pragma once

#include "telemetry_packet.h"

// Initialize ESP-NOW in station mode
void comms_init();

// Send a telemetry packet (non-blocking, async)
void comms_send_telemetry(const TelemetryPacket& pkt);

// Check if a new command has been received. Returns true and fills cmd if so.
bool comms_get_command(CommandPacket& cmd);

// Generate a new random challenge token (call on ARM)
uint16_t comms_new_challenge_token();

// Get the current challenge token
uint16_t comms_get_challenge_token();

// Time of last received packet (millis)
uint32_t comms_get_last_rx_ms();

// Print this device's MAC address to Serial
void comms_print_mac();

// Sweep WiFi channels looking for the ground station. First tries the
// channel saved in NVS; if no DISCOVERY_REPLY arrives, cycles through
// channels 1..13. On success, persists the winning channel via
// prefs_set_channel() and returns true. On failure (no reply anywhere),
// restores the previously saved channel and returns false.
// Blocks for up to ~3 seconds in the worst case.
bool comms_discover_channel();
