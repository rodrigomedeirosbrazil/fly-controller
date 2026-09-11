#include "BleServerHost.h"
#include "../config.h"
#include "../BleControl/BleControl.h"
#include <esp_bt.h>

namespace {

class HostServerCallbacks : public BLEServerCallbacks {
public:
    explicit HostServerCallbacks(BleServerHost* host) : host_(host) {}

    void onConnect(BLEServer* server) override {
        Serial.println("BLE connected");
        host_->onConnectionChanged();
    }

    void onDisconnect(BLEServer* server) override {
        Serial.println("BLE disconnected");
        // Auth is per connection. A reconnecting central starts locked.
        bleControl.onCentralDisconnected();
        host_->onConnectionChanged();
    }

private:
    BleServerHost* host_;
};

} // namespace

void BleServerHost::init(const char* deviceName) {
    BLEDevice::init(deviceName);

    // Cap BLE TX power. The ESP32-C3 Supermini browns out under full-power
    // radio (WiFi TX is already pinned to 8.5 dBm), and the default BLE power
    // compounds the current spike now that an always-on BMS connection shares
    // the radio with the advertiser/notifier. All peers (BMS, phone, remote)
    // sit within ~2 m, so 0 dBm leaves a large link-budget margin. DEFAULT
    // also applies to connection handles that aren't set explicitly.
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_N0);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV,     ESP_PWR_LVL_N0);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN,    ESP_PWR_LVL_N0);

    server_ = BLEDevice::createServer();
    if (server_ == nullptr) {
        Serial.println("[BleServerHost] WARNING: createServer() failed -- BLE unavailable");
        return;
    }
    server_->setCallbacks(new HostServerCallbacks(this));
}

void BleServerHost::handle() {
    if (!connectionChanged_) {
        return;
    }
    // Cleared before acting, not after: an edge arriving during the restart
    // below is covered by that same restart, and one arriving after it sets
    // the flag again for the next tick.
    connectionChanged_ = false;

    if (advertisingEnabled_) {
        BLEDevice::startAdvertising();
    }
}

void BleServerHost::setAdvertisingEnabled(bool enabled) {
    if (enabled == advertisingEnabled_) {
        return;
    }
    advertisingEnabled_ = enabled;

    if (enabled) {
        BLEDevice::startAdvertising();
    } else {
        BLEDevice::stopAdvertising();
    }
}
