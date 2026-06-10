#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <string.h>
#include "RemoteEspNow.h"

RemoteEspNow remoteEspNow;

static void onRecvTrampoline(const uint8_t *mac, const uint8_t *data, int len) {
    remoteEspNow.onReceive(mac, data, len);
}

void RemoteEspNow::setup() {
    WiFi.mode(WIFI_STA);
    // Same C3 Supermini stability workaround as the controller; close range.
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    esp_wifi_set_channel(REMOTE_LINK_CHANNEL, WIFI_SECOND_CHAN_NONE);

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW init failed");
        return;
    }
    esp_now_register_recv_cb(onRecvTrampoline);
    setBroadcastPeer();
}

void RemoteEspNow::setBroadcastPeer() {
    static const uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    setPeer(bcast);
}

void RemoteEspNow::setPeer(const uint8_t mac[6]) {
    if (esp_now_is_peer_exist(peerMac_)) {
        esp_now_del_peer(peerMac_);
    }
    memcpy(peerMac_, mac, 6);
    esp_now_peer_info_t peer{};
    memcpy(peer.peer_addr, peerMac_, 6);
    peer.channel = REMOTE_LINK_CHANNEL;
    peer.encrypt = false;
    esp_now_add_peer(&peer);
}

void RemoteEspNow::sendThrottle(const ThrottleToControllerPacket &pkt) {
    esp_now_send(peerMac_, reinterpret_cast<const uint8_t *>(&pkt), sizeof(pkt));
}

void RemoteEspNow::onReceive(const uint8_t *senderMac, const uint8_t *data, int len) {
    if (len != static_cast<int>(sizeof(ControllerToThrottlePacket))) return;
    memcpy(&state_, data, sizeof(state_));
    memcpy(lastSenderMac_, senderMac, 6);
    lastRxMs_ = millis();
    hasState_ = true;
}
