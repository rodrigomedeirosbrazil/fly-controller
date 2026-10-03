#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_system.h>
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

void RemoteLink::setupRadio() {
    // Station mode, never associated: the canonical ESP-NOW setup, with no AP
    // beaconing for nothing.
    WiFi.mode(WIFI_STA);
    // The ESP32-C3 Supermini is unstable at full TX power (commit f06aa0d).
    WiFi.setTxPower(WIFI_POWER_8_5dBm);

    // Paired remotes learned this controller's MAC from packets sent on the
    // softAP interface (base MAC + 1). Carry that address over to STA so they
    // keep working without re-pairing.
    uint8_t apMac[6];
    uint8_t staMac[6];
    const bool moved = esp_read_mac(apMac, ESP_MAC_WIFI_SOFTAP) == ESP_OK &&
                       esp_wifi_set_mac(WIFI_IF_STA, apMac) == ESP_OK &&
                       esp_wifi_get_mac(WIFI_IF_STA, staMac) == ESP_OK &&
                       memcmp(apMac, staMac, 6) == 0;
    if (!moved) {
        Serial.println("[RemoteLink] could not move the softAP MAC onto STA -- "
                       "paired remotes must be re-paired");
    }

    esp_wifi_set_channel(REMOTE_LINK_CHANNEL, WIFI_SECOND_CHAN_NONE);
}

void RemoteLink::setup() {
    setupRadio();
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

void RemoteLink::forgetPeer() {
    static const uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    if (esp_now_is_peer_exist(peerMac_)) {
        esp_now_del_peer(peerMac_);
    }

    // Back to the broadcast peer setup() uses when nothing is paired, so the
    // link is ready to hear a new remote without a reboot.
    memcpy(peerMac_, bcast, 6);
    addPeer(peerMac_);

    // Drop the last received packet too: hasState_ is what isLinkFresh() and
    // the throttle failsafe key off, and a stale true would keep the wireless
    // source alive for one more freshness window against a remote we just
    // forgot.
    hasState_ = false;
    rx_ = ThrottleToControllerPacket{};
}

void RemoteLink::addPeer(const uint8_t mac[6]) {
    if (esp_now_is_peer_exist(mac)) esp_now_del_peer(mac);
    esp_now_peer_info_t peer{};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = REMOTE_LINK_CHANNEL;
    peer.ifidx = WIFI_IF_STA; // STA, carrying the old softAP MAC (setupRadio)
    peer.encrypt = false;
    esp_now_add_peer(&peer);
}

void RemoteLink::requestBeep(uint8_t beep) {
    tx_.beepCommand = beep;
    tx_.beepCommandCounter++;
    sendState(); // send immediately so the remote hears it without waiting for the next 5 Hz tick
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
