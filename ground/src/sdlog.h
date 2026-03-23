#pragma once

#include "telemetry_packet.h"
#include <stdint.h>

// Initialize SD card. Returns true if SD is ready.
bool sdlog_init();

// Start a new log file (call on ARM). Returns true if file opened.
bool sdlog_start_flight();

// Enqueue a telemetry packet for logging (non-blocking, uses ring buffer)
void sdlog_enqueue(const TelemetryPacket& pkt, uint32_t ground_ms, int8_t rssi);

// Write buffered rows to SD. Call frequently from main loop.
// Writes up to max_rows per call to avoid blocking too long.
void sdlog_flush_some(int max_rows = 10);

// Flush all remaining data and close the file (call on LANDED)
void sdlog_end_flight();

// Is SD card ready?
bool sdlog_is_ready();

// Number of rows written to current file
uint32_t sdlog_rows_written();

// Number of write errors
uint32_t sdlog_write_errors();
