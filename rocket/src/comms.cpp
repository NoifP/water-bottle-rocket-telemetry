#include "comms.h"
#include "config.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

static volatile bool new_command = false;
static CommandPacket received_cmd;
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

    // Copy to buffer
    memcpy(&received_cmd, cmd, sizeof(CommandPacket));
    new_command = true;
    last_rx_ms = millis();
}

// ESP-NOW send callback (for debugging/link quality)
static void on_data_sent(const uint8_t* mac, esp_now_send_status_t status) {
    // Could track send success rate here if needed
    (void)mac;
    (void)status;
}

void comms_init() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    // Set WiFi channel
    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

    if (esp_now_init() != ESP_OK) {
        Serial.println("[comms] ESP-NOW init FAILED");
        return;
    }

    esp_now_register_recv_cb(on_data_recv);
    esp_now_register_send_cb(on_data_sent);

    // Register ground station as peer
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, GROUND_MAC, 6);
    peer.channel = ESPNOW_CHANNEL;
    peer.encrypt = false;

    if (esp_now_add_peer(&peer) != ESP_OK) {
        Serial.println("[comms] Failed to add ground station peer");
    }

    Serial.println("[comms] ESP-NOW initialized");
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
