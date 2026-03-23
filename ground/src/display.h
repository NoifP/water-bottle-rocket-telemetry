#pragma once

#include "telemetry_packet.h"
#include <stdint.h>

// Application state shared across modules
struct AppState {
    TelemetryPacket lastPacket;
    bool    hasReceivedPacket;
    bool    isArmed;

    // Signal quality
    uint32_t packetsReceived;
    uint32_t packetsThisSecond;
    uint32_t lastPacketMs;
    bool     signalLost;

    // Flight timing
    uint32_t flightStartMs;
    bool     flightActive;

    // SD
    bool     sdReady;
    uint32_t sdRows;
    uint32_t sdErrors;

    // Confirmation dialog
    bool     confirmActive;
    uint32_t confirmStartMs;
};

// Initialize the TFT display
void display_init();

// Draw the full static layout (labels, dividers, button outlines)
void display_draw_layout();

// Update dynamic values on screen. Call at 10 Hz.
void display_update(const AppState& state);

// Draw the confirmation dialog overlay
void display_draw_confirm(const char* message);

// Clear the confirmation dialog
void display_clear_confirm();

// Draw the ARM/DISARM button in the correct state
void display_update_arm_button(bool is_armed);

// Draw the deploy button (enabled/disabled)
void display_update_deploy_button(bool enabled);
