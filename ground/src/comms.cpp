#include "comms.h"
#include "config.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

static volatile bool new_packet = false;
static TelemetryPacket rx_packet;
static int8_t last_rssi = 0;
static uint32_t last_rx_ms = 0;

// ESP-NOW receive callback
static void on_data_recv(const uint8_t* mac, const uint8_t* data, int len) {
    if (len != sizeof(TelemetryPacket)) return;

    const TelemetryPacket* pkt = reinterpret_cast<const TelemetryPacket*>(data);

    // Validate magic byte
    if (pkt->magic != TELEMETRY_MAGIC) return;

    // Validate checksum
    uint8_t expected = compute_checksum(data, sizeof(TelemetryPacket) - 1);
    if (pkt->checksum != expected) return;

    // Copy to buffer
    memcpy(&rx_packet, pkt, sizeof(TelemetryPacket));
    new_packet = true;
    last_rx_ms = millis();
}

// ESP-NOW send callback
static void on_data_sent(const uint8_t* mac, esp_now_send_status_t status) {
    (void)mac;
    (void)status;
}

void comms_init() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

    if (esp_now_init() != ESP_OK) {
        Serial.println("[comms] ESP-NOW init FAILED");
        return;
    }

    esp_now_register_recv_cb(on_data_recv);
    esp_now_register_send_cb(on_data_sent);

    // Register rocket as peer for sending commands
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, ROCKET_MAC, 6);
    peer.channel = ESPNOW_CHANNEL;
    peer.encrypt = false;

    if (esp_now_add_peer(&peer) != ESP_OK) {
        Serial.println("[comms] Failed to add rocket peer");
    }

    Serial.println("[comms] ESP-NOW initialized");
    comms_print_mac();
}

bool comms_has_new_packet() {
    return new_packet;
}

TelemetryPacket comms_get_packet() {
    new_packet = false;
    return rx_packet;
}

int8_t comms_get_rssi() {
    return last_rssi;
}

void comms_send_command(Command cmd, uint16_t token) {
    CommandPacket pkt = {};
    pkt.magic = COMMAND_MAGIC;
    pkt.command = cmd;
    pkt.token = token;
    pkt.checksum = compute_checksum(&pkt, sizeof(CommandPacket) - 1);

    esp_now_send(ROCKET_MAC, (const uint8_t*)&pkt, sizeof(CommandPacket));
    Serial.printf("[comms] Sent command %d with token 0x%04X\n", (int)cmd, token);
}

uint32_t comms_get_last_rx_ms() {
    return last_rx_ms;
}

void comms_print_mac() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    Serial.printf("[comms] Ground MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
