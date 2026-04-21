#pragma once

#include "telemetry_packet.h"

// Initialize ESP-NOW in station mode
void comms_init();

// Check if a new telemetry packet has been received
bool comms_has_new_packet();

// Get the latest telemetry packet (clears the new flag)
TelemetryPacket comms_get_packet();

// Get RSSI of last received packet
int8_t comms_get_rssi();

// Send a command to the rocket
void comms_send_command(Command cmd, uint16_t token);

// Time of last received packet (millis)
uint32_t comms_get_last_rx_ms();

// Print this device's MAC address to Serial
void comms_print_mac();

// Change the ESP-NOW WiFi channel at runtime, re-register the peer, and
// persist the new value via prefs_set_channel(). Caller should already have
// stopped any active SoftAP (the AP is re-started on its own channel).
void comms_set_channel(uint8_t ch);

// Re-apply the currently configured channel to the radio. Useful after
// WiFi.scanNetworks() or similar operations that may have hopped channels.
void comms_reapply_channel();
