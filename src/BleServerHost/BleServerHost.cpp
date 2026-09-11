#include "BleServerHost.h"
#include "../config.h"
#include <esp_bt.h>

namespace {

class HostServerCallbacks : public BLEServerCallbacks {
public:
    explicit HostServerCallbacks(BleServerHost* host) : host_(host) {}

    void onConnect(BLEServer* server) override {
        // Bluedroid stops advertising as soon as the first central connects.
        // Without restarting it here a second central can never discover the
        // controller, which is exactly the XCTrack + fly-app case.
        host_->refreshAdvertising();
    }

    void onDisconnect(BLEServer* server) override {
        host_->refreshAdvertising();
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
    server_->setCallbacks(new HostServerCallbacks(this));
}

uint8_t BleServerHost::getConnectedCount() const {
    if (server_ == nullptr) {
        return 0;
    }
    return (uint8_t) server_->getConnectedCount();
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

void BleServerHost::refreshAdvertising() {
    if (!advertisingEnabled_) {
        return;
    }
    BLEDevice::startAdvertising();
}
