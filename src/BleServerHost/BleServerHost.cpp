#include "BleServerHost.h"
#include "../config.h"
#include "../BleControl/BleControl.h"
#include "BleLinkPolicy.h"
#include <esp_bt.h>
#include <esp_gap_ble_api.h>

namespace {

// Touched only from Bluedroid-task callbacks (GATTS connect/disconnect and GAP
// events are delivered on the same BTC task), so it needs no lock. The loop
// task reads only the atomic count published through the host.
BleLinkPolicy::PeripheralLinks gPeripheralLinks;

void requestPhoneConnParams(esp_bd_addr_t bda) {
    esp_ble_conn_update_params_t params = {};
    memcpy(params.bda, bda, sizeof(esp_bd_addr_t));
    params.min_int = BleLinkPolicy::kPhoneConnParams.minInterval;
    params.max_int = BleLinkPolicy::kPhoneConnParams.maxInterval;
    params.latency = BleLinkPolicy::kPhoneConnParams.latency;
    params.timeout = BleLinkPolicy::kPhoneConnParams.timeout;
    esp_ble_gap_update_conn_params(&params);
}

// Re-asks once when the phone's own renegotiation lands under the 6 s timeout.
void onGapEvent(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param) {
    if (event != ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT || param == nullptr) {
        return;
    }
    if (param->update_conn_params.status != ESP_BT_STATUS_SUCCESS) {
        return;
    }
    if (gPeripheralLinks.shouldReRequest(param->update_conn_params.bda,
                                         param->update_conn_params.timeout)) {
        requestPhoneConnParams(param->update_conn_params.bda);
    }
}

class HostServerCallbacks : public BLEServerCallbacks {
public:
    explicit HostServerCallbacks(BleServerHost* host) : host_(host) {}

    // The library calls both onConnect overloads; only this param overload
    // acts, because it carries the link role and the peer address.
    void onConnect(BLEServer* server, esp_ble_gatts_cb_param_t* param) override {
        if (param == nullptr || param->connect.link_role != BleLinkPolicy::kLinkRolePeripheral) {
            return;  // a BMS link (we are the central) is not a phone
        }
        Serial.println("BLE connected");
        gPeripheralLinks.onConnect(param->connect.remote_bda, param->connect.link_role);
        host_->publishPeripheralLinkCount(gPeripheralLinks.count());
        requestPhoneConnParams(param->connect.remote_bda);
        host_->onConnectionChanged();
    }

    // The param overload: conn_id says WHICH central left, which the auth
    // reset needs. The library calls both overloads, so the plain one is left
    // to the no-op base rather than duplicating the work here.
    void onDisconnect(BLEServer* server, esp_ble_gatts_cb_param_t* param) override {
        if (param == nullptr || param->disconnect.link_role != BleLinkPolicy::kLinkRolePeripheral) {
            return;  // a BMS link dropping is none of the phone's business
        }
        Serial.println("BLE disconnected");
        bleControl.onCentralDisconnected(param->disconnect.conn_id);
        // Forget the MTU when the last phone leaves: replies are sized from
        // it, and a stale larger value would make them exceed the next
        // phone's link. Counting peripheral links only keeps a live BMS link
        // from masking that.
        if (gPeripheralLinks.onDisconnect(param->disconnect.remote_bda, param->disconnect.link_role)) {
            host_->resetNegotiatedMtu();
        }
        host_->publishPeripheralLinkCount(gPeripheralLinks.count());
        host_->onConnectionChanged();
    }

    void onMtuChanged(BLEServer* server, esp_ble_gatts_cb_param_t* param) override {
        if (param != nullptr) {
            host_->onMtuNegotiated(param->mtu.mtu);
        }
    }

private:
    BleServerHost* host_;
};

} // namespace

void BleServerHost::init(const char* deviceName) {
    BLEDevice::init(deviceName);
    BLEDevice::setCustomGapHandler(onGapEvent);

    // BLE TX power. +9 dBm is the precompiled Arduino C3 default
    // (CONFIG_BT_CTRL_DFT_TX_POWER_LEVEL_P9), set explicitly here for clarity.
    // Higher levels risk a brownout on the ESP32-C3 Supermini and were not
    // measured. Lower power (0 dBm) proved too little for the phone: the
    // pilot's body and a pocket cost far more than free-space distance.
    // Scan stays at 0 dBm since it only carries scan requests. DEFAULT also
    // applies to connection handles that aren't set explicitly.
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV,     ESP_PWR_LVL_P9);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN,    ESP_PWR_LVL_N0);

    server_ = BLEDevice::createServer();
    if (server_ == nullptr) {
        Serial.println("[BleServerHost] WARNING: createServer() failed -- BLE unavailable");
        return;
    }
    server_->setCallbacks(new HostServerCallbacks(this));
}

void BleServerHost::startAdvertisingWithPolicy() {
    const BleLinkPolicy::AdvInterval interval =
        BleLinkPolicy::advIntervalFor(peripheralLinkCount_.load());
    BLEAdvertising* advertising = BLEDevice::getAdvertising();
    if (interval.min != appliedAdvMin_ || interval.max != appliedAdvMax_) {
        // The new interval only takes effect on a fresh start.
        BLEDevice::stopAdvertising();
        advertising->setMinInterval(interval.min);
        advertising->setMaxInterval(interval.max);
        appliedAdvMin_ = interval.min;
        appliedAdvMax_ = interval.max;
    }
    BLEDevice::startAdvertising();
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
        startAdvertisingWithPolicy();
    }
}

void BleServerHost::setAdvertisingEnabled(bool enabled) {
    if (enabled == advertisingEnabled_) {
        return;
    }
    advertisingEnabled_ = enabled;

    if (enabled) {
        startAdvertisingWithPolicy();
    } else {
        BLEDevice::stopAdvertising();
    }
}
