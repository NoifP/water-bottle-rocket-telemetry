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
