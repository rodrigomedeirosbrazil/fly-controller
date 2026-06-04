#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <string.h>
#include "RemoteLink.h"
#include "../Settings/Settings.h"

extern Settings settings;

// remoteLink is defined in config.cpp (single-translation-unit convention).

static const uint16_t STATE_TX_INTERVAL_MS = 200; // ~5 Hz heartbeat

static void onRecvTrampoline(const uint8_t *mac, const uint8_t *data, int len) {
    remoteLink.onReceive(mac, data, len);
}

// Parse "AA:BB:CC:DD:EE:FF" into 6 bytes. Returns true on success.
static bool parseMac(const String &s, uint8_t out[6]) {
    if (s.length() != 17) return false;
    for (int i = 0; i < 6; i++) {
        out[i] = (uint8_t)strtoul(s.substring(i * 3, i * 3 + 2).c_str(), nullptr, 16);
    }
    return true;
}

void RemoteLink::setup() {
    // WiFi AP is already up (webServer.begin()); ESP-NOW rides the same radio.
    if (esp_now_init() != ESP_OK) {
        Serial.println("[RemoteLink] ESP-NOW init failed");
        return;
    }
    esp_now_register_recv_cb(onRecvTrampoline);

    uint8_t mac[6];
    String saved = settings.getRemoteMac();
    if (saved.length() == 17 && parseMac(saved, mac)) {
        memcpy(peerMac_, mac, 6);
    } else {
        static const uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        memcpy(peerMac_, bcast, 6);
    }
    addPeer(peerMac_);
}

void RemoteLink::addPeer(const uint8_t mac[6]) {
    if (esp_now_is_peer_exist(mac)) esp_now_del_peer(mac);
    esp_now_peer_info_t peer{};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = REMOTE_LINK_CHANNEL;
    peer.encrypt = false;
    esp_now_add_peer(&peer);
}

void RemoteLink::requestBeep(uint8_t beep) {
    tx_.beepCommand = beep;
    tx_.beepCommandCounter++; // wraps; remote acts on change
}

void RemoteLink::sendState() {
    esp_now_send(peerMac_, reinterpret_cast<const uint8_t *>(&tx_), sizeof(tx_));
}

void RemoteLink::handle() {
    uint32_t now = millis();
    if (now - lastTxMs_ >= STATE_TX_INTERVAL_MS) {
        lastTxMs_ = now;
        sendState();
    }
}

void RemoteLink::onReceive(const uint8_t *senderMac, const uint8_t *data, int len) {
    if (len != static_cast<int>(sizeof(ThrottleToControllerPacket))) return;

    // During pairing, the first remote we hear becomes our peer.
    if (pairing_) {
        char macStr[18];
        snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                 senderMac[0], senderMac[1], senderMac[2],
                 senderMac[3], senderMac[4], senderMac[5]);
        settings.setRemoteMac(macStr);
        settings.save();
        memcpy(peerMac_, senderMac, 6);
        addPeer(peerMac_);
        pairing_ = false;
    }

    memcpy(&rx_, data, sizeof(rx_));
    lastRxMs_ = millis();
    hasState_ = true;
}
