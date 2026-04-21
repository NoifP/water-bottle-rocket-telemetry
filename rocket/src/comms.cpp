#include "comms.h"
#include "config.h"
#include "prefs.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

static volatile bool new_command = false;
static CommandPacket received_cmd;
static volatile bool got_discovery_reply = false;
static uint16_t challenge_token = 0;
static uint32_t last_rx_ms = 0;

// ESP-NOW receive callback
static void on_data_recv(const uint8_t* mac, const uint8_t* data, int len) {
    if (len != sizeof(CommandPacket)) return;

    const CommandPacket* cmd = reinterpret_cast<const CommandPacket*>(data);

    // Validate magic byte
    if (cmd->magic != COMMAND_MAGIC) return;

    // Validate checksum
    uint8_t expected = compute_checksum(data, sizeof(CommandPacket) - 1);
    if (cmd->checksum != expected) return;

    last_rx_ms = millis();

    if (cmd->command == Command::DISCOVERY_REPLY) {
        got_discovery_reply = true;
        return;
    }

    // Copy to buffer
    memcpy(&received_cmd, cmd, sizeof(CommandPacket));
    new_command = true;
}

// ESP-NOW send callback (for debugging/link quality)
static void on_data_sent(const uint8_t* mac, esp_now_send_status_t status) {
    // Could track send success rate here if needed
    (void)mac;
    (void)status;
}

static void apply_channel(uint8_t ch) {
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    // Re-register the peer on the new channel. Delete first since
    // esp_now_add_peer() fails if the peer already exists.
    esp_now_del_peer(GROUND_MAC);
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, GROUND_MAC, 6);
    peer.channel = ch;
    peer.encrypt = false;
    esp_now_add_peer(&peer);
}

void comms_init() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    uint8_t ch = prefs_get_channel();
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);

    if (esp_now_init() != ESP_OK) {
        Serial.println("[comms] ESP-NOW init FAILED");
        return;
    }

    esp_now_register_recv_cb(on_data_recv);
    esp_now_register_send_cb(on_data_sent);

    // Register ground station as peer
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, GROUND_MAC, 6);
    peer.channel = ch;
    peer.encrypt = false;

    if (esp_now_add_peer(&peer) != ESP_OK) {
        Serial.println("[comms] Failed to add ground station peer");
    }

    Serial.printf("[comms] ESP-NOW initialized on channel %u\n", (unsigned)ch);
    comms_print_mac();
}

void comms_send_telemetry(const TelemetryPacket& pkt) {
    esp_now_send(GROUND_MAC, (const uint8_t*)&pkt, sizeof(TelemetryPacket));
}

bool comms_get_command(CommandPacket& cmd) {
    if (!new_command) return false;
    new_command = false;
    cmd = received_cmd;
    return true;
}

uint16_t comms_new_challenge_token() {
    challenge_token = (uint16_t)(esp_random() & 0xFFFF);
    if (challenge_token == 0) challenge_token = 1; // avoid 0
    Serial.printf("[comms] New challenge token: 0x%04X\n", challenge_token);
    return challenge_token;
}

uint16_t comms_get_challenge_token() {
    return challenge_token;
}

uint32_t comms_get_last_rx_ms() {
    return last_rx_ms;
}

void comms_print_mac() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    Serial.printf("[comms] Rocket MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// --- Discovery sweep -------------------------------------------------------

// Send a minimal FLAG_DISCOVERY-tagged telemetry packet. Sensor fields are
// zeroed; the ground only cares about the magic, version, and flag bit.
static void send_discovery_probe() {
    TelemetryPacket pkt = {};
    pkt.magic = TELEMETRY_MAGIC;
    pkt.version = PROTOCOL_VERSION;
    pkt.seq = 0;
    pkt.timestamp_ms = millis();
    pkt.state = FlightState::IDLE;
    pkt.flags = FLAG_DISCOVERY;
    pkt.checksum = compute_checksum(&pkt, sizeof(TelemetryPacket) - 1);
    esp_now_send(GROUND_MAC, (const uint8_t*)&pkt, sizeof(TelemetryPacket));
}

// Probe the currently-set channel: send a few discovery packets and wait
// for a DISCOVERY_REPLY. Returns true if a reply arrived within ~window_ms.
static bool probe_current_channel(uint16_t window_ms, uint8_t n_pings) {
    got_discovery_reply = false;
    uint32_t per_ping = window_ms / (n_pings + 1);
    for (uint8_t i = 0; i < n_pings; i++) {
        send_discovery_probe();
        uint32_t t0 = millis();
        while (millis() - t0 < per_ping) {
            if (got_discovery_reply) return true;
            delay(5);
        }
    }
    // Final listen window
    uint32_t t0 = millis();
    while (millis() - t0 < per_ping) {
        if (got_discovery_reply) return true;
        delay(5);
    }
    return false;
}

bool comms_discover_channel() {
    uint8_t saved = prefs_get_channel();

    Serial.printf("[comms] Discovery: trying saved channel %u\n", (unsigned)saved);
    if (probe_current_channel(300, 3)) {
        Serial.println("[comms] Discovery: locked on saved channel");
        return true;
    }

    Serial.println("[comms] Discovery: saved channel silent, sweeping 1..13");
    for (uint8_t ch = 1; ch <= 13; ch++) {
        if (ch == saved) continue;
        apply_channel(ch);
        if (probe_current_channel(200, 2)) {
            Serial.printf("[comms] Discovery: found ground on channel %u\n", (unsigned)ch);
            prefs_set_channel(ch);
            return true;
        }
    }

    // Nothing found anywhere — restore radio to the saved channel so
    // the rocket keeps transmitting where it was last successful.
    apply_channel(saved);
    Serial.println("[comms] Discovery: no reply; staying on saved channel");
    return false;
}
