#include "sdlog.h"
#include "config.h"
#include <SD.h>
#include <SPI.h>

struct LogEntry {
    TelemetryPacket pkt;
    uint32_t ground_ms;
    int8_t rssi;
};

static LogEntry ring_buffer[SD_RING_BUFFER_SIZE];
static volatile uint16_t ring_head = 0; // write position
static uint16_t ring_tail = 0;          // read position

static File log_file;
static bool sd_ready = false;
static bool file_open = false;
static uint32_t rows_written = 0;
static uint32_t write_errors = 0;
static uint32_t last_flush_ms = 0;

static int find_next_file_number() {
    int max_num = 0;
    File root = SD.open("/");
    if (!root) return 1;

    File f = root.openNextFile();
    while (f) {
        String name = f.name();
        if (name.startsWith("flight_") && name.endsWith(".csv")) {
            // Extract number from "flight_NNNN.csv"
            String numStr = name.substring(7, name.length() - 4);
            int num = numStr.toInt();
            if (num > max_num) max_num = num;
        }
        f = root.openNextFile();
    }
    root.close();
    return max_num + 1;
}

bool sdlog_init() {
    if (!SD.begin(SD_CS)) {
        Serial.println("[sdlog] SD card init FAILED");
        sd_ready = false;
        return false;
    }

    uint64_t total = SD.totalBytes();
    uint64_t used = SD.usedBytes();
    Serial.printf("[sdlog] SD card OK. %llu MB total, %llu MB used\n",
                  total / (1024 * 1024), used / (1024 * 1024));

    sd_ready = true;
    return true;
}

bool sdlog_start_flight() {
    if (!sd_ready) return false;
    if (file_open) sdlog_end_flight();

    int num = find_next_file_number();
    char filename[32];
    snprintf(filename, sizeof(filename), "/flight_%04d.csv", num);

    log_file = SD.open(filename, FILE_WRITE);
    if (!log_file) {
        Serial.printf("[sdlog] Failed to create %s\n", filename);
        return false;
    }

    // Write CSV header
    log_file.println(
        "ground_ms,rocket_ms,altitude_m,speed_mps,accel_g,"
        "accel_x,accel_y,accel_z,pressure_pa,temp_c,humidity_pct,"
        "state,chute,batt_v,rssi"
    );

    file_open = true;
    rows_written = 0;
    ring_head = 0;
    ring_tail = 0;
    last_flush_ms = millis();

    Serial.printf("[sdlog] Logging to %s\n", filename);
    return true;
}

void sdlog_enqueue(const TelemetryPacket& pkt, uint32_t ground_ms, int8_t rssi) {
    if (!file_open) return;

    uint16_t next_head = (ring_head + 1) % SD_RING_BUFFER_SIZE;
    if (next_head == ring_tail) {
        // Buffer full -- drop oldest
        ring_tail = (ring_tail + 1) % SD_RING_BUFFER_SIZE;
        write_errors++;
    }

    ring_buffer[ring_head].pkt = pkt;
    ring_buffer[ring_head].ground_ms = ground_ms;
    ring_buffer[ring_head].rssi = rssi;
    ring_head = next_head;
}

void sdlog_flush_some(int max_rows) {
    if (!file_open) return;

    int written = 0;
    while (ring_tail != ring_head && written < max_rows) {
        const LogEntry& entry = ring_buffer[ring_tail];
        const TelemetryPacket& p = entry.pkt;

        // Format CSV row
        char buf[256];
        int len = snprintf(buf, sizeof(buf),
            "%lu,%lu,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.1f,%.1f,%.1f,%d,%d,%.2f,%d\n",
            (unsigned long)entry.ground_ms,
            (unsigned long)p.timestamp_ms,
            p.altitude_m,
            p.vertical_speed,
            p.accel_magnitude / 9.81f, // Convert to g
            p.accel_x, p.accel_y, p.accel_z,
            p.pressure_pa,
            p.temperature_c,
            p.humidity_pct,
            (int)p.state,
            (p.flags & FLAG_CHUTE_DEPLOYED) ? 1 : 0,
            p.battery_v,
            (int)entry.rssi
        );

        if (log_file.write((const uint8_t*)buf, len) == (size_t)len) {
            rows_written++;
        } else {
            write_errors++;
        }

        ring_tail = (ring_tail + 1) % SD_RING_BUFFER_SIZE;
        written++;
    }

    // Periodic flush to card
    uint32_t now = millis();
    if (now - last_flush_ms >= SD_FLUSH_INTERVAL_MS) {
        log_file.flush();
        last_flush_ms = now;
    }
}

void sdlog_end_flight() {
    if (!file_open) return;

    // Flush all remaining buffered data
    while (ring_tail != ring_head) {
        sdlog_flush_some(SD_RING_BUFFER_SIZE);
    }

    log_file.flush();
    log_file.close();
    file_open = false;
    Serial.printf("[sdlog] Flight log closed. %lu rows written.\n", (unsigned long)rows_written);
}

bool sdlog_is_ready() { return sd_ready; }
uint32_t sdlog_rows_written() { return rows_written; }
uint32_t sdlog_write_errors() { return write_errors; }
