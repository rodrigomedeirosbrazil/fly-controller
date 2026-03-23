#include "BluetoothBms.h"
#include "../config.h"
#include "../DalyBms/DalyBms.h"
#include "../JbdBms/JbdBms.h"
#include "../Settings/Settings.h"
#include "../Xctod/Xctod.h"
#include <BLEAdvertisedDevice.h>
#include <BLEDevice.h>

namespace {
constexpr char JBD_SCAN_SERVICE_UUID[] = "0000ff00-0000-1000-8000-00805f9b34fb";
constexpr char DALY_SCAN_SERVICE_UUID[] = "0000fff0-0000-1000-8000-00805f9b34fb";
constexpr uint32_t WEB_SCAN_DURATION_SECONDS = 5;
} // namespace

void BluetoothBms::init() {
    jbdBms.init();
    dalyBms.init();
    clearWebScanResults();
}

void BluetoothBms::update() {
    if (isWebScanBusy()) {
        return;
    }

    const uint8_t activeType = getActiveType();
    const String mac = settings.getBmsMac();

    jbdBms.setMacAddress(mac);
    jbdBms.setEnabled(activeType == BmsTypeJbd);

    dalyBms.setMacAddress(mac);
    dalyBms.setEnabled(activeType == BmsTypeDaly);

    if (activeType == BmsTypeJbd) {
        jbdBms.update();
    } else if (activeType == BmsTypeDaly) {
        dalyBms.update();
    }
}

bool BluetoothBms::isConnected() const {
    const uint8_t activeType = getActiveType();
    if (activeType == BmsTypeJbd) {
        return jbdBms.isConnected();
    }
    if (activeType == BmsTypeDaly) {
        return dalyBms.isConnected();
    }
    return false;
}

bool BluetoothBms::hasData() const {
    const uint8_t activeType = getActiveType();
    if (activeType == BmsTypeJbd) {
        return jbdBms.hasData();
    }
    if (activeType == BmsTypeDaly) {
        return dalyBms.hasData();
    }
    return false;
}

bool BluetoothBms::hasCellData() const {
    const uint8_t activeType = getActiveType();
    if (activeType == BmsTypeJbd) {
        return jbdBms.hasCellData();
    }
    if (activeType == BmsTypeDaly) {
        return dalyBms.hasCellData();
    }
    return false;
}

uint32_t BluetoothBms::getPackVoltageMilliVolts() const {
    const uint8_t activeType = getActiveType();
    if (activeType == BmsTypeJbd) {
        return jbdBms.getPackVoltageMilliVolts();
    }
    if (activeType == BmsTypeDaly) {
        return dalyBms.getPackVoltageMilliVolts();
    }
    return 0;
}

int32_t BluetoothBms::getPackCurrentMilliAmps() const {
    const uint8_t activeType = getActiveType();
    if (activeType == BmsTypeJbd) {
        return jbdBms.getPackCurrentMilliAmps();
    }
    if (activeType == BmsTypeDaly) {
        return dalyBms.getPackCurrentMilliAmps();
    }
    return 0;
}

uint8_t BluetoothBms::getSoCPercent() const {
    const uint8_t activeType = getActiveType();
    if (activeType == BmsTypeJbd) {
        return jbdBms.getSoCPercent();
    }
    if (activeType == BmsTypeDaly) {
        return dalyBms.getSoCPercent();
    }
    return 0;
}

uint8_t BluetoothBms::getCellCount() const {
    const uint8_t activeType = getActiveType();
    if (activeType == BmsTypeJbd) {
        return jbdBms.getCellCount();
    }
    if (activeType == BmsTypeDaly) {
        return dalyBms.getCellCount();
    }
    return 0;
}

uint8_t BluetoothBms::getTempCount() const {
    const uint8_t activeType = getActiveType();
    if (activeType == BmsTypeJbd) {
        return jbdBms.getNtcCount();
    }
    if (activeType == BmsTypeDaly) {
        return dalyBms.getTempCount();
    }
    return 0;
}

int16_t BluetoothBms::getTempCelsius(uint8_t index) const {
    const uint8_t activeType = getActiveType();
    if (activeType == BmsTypeJbd) {
        return jbdBms.getNtcTempCelsius(index);
    }
    if (activeType == BmsTypeDaly) {
        return dalyBms.getTempCelsius(index);
    }
    return 0;
}

uint16_t BluetoothBms::getCellVoltageMilliVolts(uint8_t index) const {
    const uint8_t activeType = getActiveType();
    if (activeType == BmsTypeJbd) {
        return jbdBms.getCellVoltageMilliVolts(index);
    }
    if (activeType == BmsTypeDaly) {
        return dalyBms.getCellVoltageMilliVolts(index);
    }
    return 0;
}

uint16_t BluetoothBms::getCellMinMilliVolts() const {
    const uint8_t activeType = getActiveType();
    if (activeType == BmsTypeJbd) {
        return jbdBms.getCellMinMilliVolts();
    }
    if (activeType == BmsTypeDaly) {
        return dalyBms.getCellMinMilliVolts();
    }
    return 0;
}

uint16_t BluetoothBms::getCellMaxMilliVolts() const {
    const uint8_t activeType = getActiveType();
    if (activeType == BmsTypeJbd) {
        return jbdBms.getCellMaxMilliVolts();
    }
    if (activeType == BmsTypeDaly) {
        return dalyBms.getCellMaxMilliVolts();
    }
    return 0;
}

uint16_t BluetoothBms::getCellDeltaMilliVolts() const {
    const uint8_t activeType = getActiveType();
    if (activeType == BmsTypeJbd) {
        return jbdBms.getCellDeltaMilliVolts();
    }
    if (activeType == BmsTypeDaly) {
        return dalyBms.getCellDeltaMilliVolts();
    }
    return 0;
}

uint8_t BluetoothBms::getActiveType() const {
    return settings.getBmsType();
}

bool BluetoothBms::startWebScan() {
    if (!BLEDevice::getInitialized()) {
        resetWebScanState(BluetoothBmsScanError);
        strlcpy(webScanError_, "BLE stack is not initialized", sizeof(webScanError_));
        return false;
    }

    if (isWebScanBusy()) {
        return true;
    }

    clearWebScanResults();
    webScanStatus_ = BluetoothBmsScanScanning;

    jbdBms.setEnabled(false);
    dalyBms.setEnabled(false);
    pauseTelemetryAdvertisingForScan();

    BLEScan* scan = BLEDevice::getScan();
    scan->stop();
    scan->clearResults();
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(99);

    if (!scan->start(WEB_SCAN_DURATION_SECONDS, BluetoothBms::onWebScanComplete, false)) {
        resetWebScanState(BluetoothBmsScanError);
        strlcpy(webScanError_, "Failed to start BLE scan", sizeof(webScanError_));
        resumeTelemetryAdvertisingAfterScan();
        return false;
    }

    return true;
}

void BluetoothBms::clearWebScanResults() {
    for (uint8_t i = 0; i < MAX_WEB_SCAN_RESULTS; i++) {
        webScanResults_[i].mac = "";
        webScanResults_[i].name = "";
        webScanResults_[i].rssi = 0;
        webScanResults_[i].detectedType = BmsTypeNone;
    }
    webScanResultCount_ = 0;
    resetWebScanState(BluetoothBmsScanIdle);
}

uint8_t BluetoothBms::getWebScanStatus() const {
    return webScanStatus_;
}

const char* BluetoothBms::getWebScanError() const {
    return webScanError_;
}

uint8_t BluetoothBms::getWebScanResultCount() const {
    return webScanResultCount_;
}

const BluetoothBmsScanResult* BluetoothBms::getWebScanResults() const {
    return webScanResults_;
}

bool BluetoothBms::isWebScanBusy() const {
    return webScanStatus_ == BluetoothBmsScanScanning;
}

void BluetoothBms::resetWebScanState(uint8_t status) {
    webScanStatus_ = status;
    webScanError_[0] = '\0';
}

void BluetoothBms::pauseTelemetryAdvertisingForScan() {
    telemetryAdvertisingPausedForScan_ = true;
    xctod.setAdvertisingEnabled(false);
}

void BluetoothBms::resumeTelemetryAdvertisingAfterScan() {
    if (!telemetryAdvertisingPausedForScan_) {
        return;
    }
    telemetryAdvertisingPausedForScan_ = false;
    xctod.setAdvertisingEnabled(true);
}

void BluetoothBms::completeWebScan() {
    BLEScan* scan = BLEDevice::getScan();
    BLEScanResults results = scan->getResults();

    for (int i = 0; i < results.getCount(); i++) {
        BLEAdvertisedDevice device = results.getDevice(i);
        uint8_t detectedType = BmsTypeNone;

        if (device.isAdvertisingService(BLEUUID(JBD_SCAN_SERVICE_UUID))) {
            detectedType = BmsTypeJbd;
        } else if (device.isAdvertisingService(BLEUUID(DALY_SCAN_SERVICE_UUID))) {
            detectedType = BmsTypeDaly;
        }

        if (detectedType == BmsTypeNone) {
            continue;
        }

        const String mac = String(device.getAddress().toString().c_str());
        const String name = device.haveName() ? String(device.getName().c_str()) : "";
        tryStoreScanResult(mac, name, device.getRSSI(), detectedType);
    }

    scan->clearResults();
    webScanStatus_ = BluetoothBmsScanComplete;
    resumeTelemetryAdvertisingAfterScan();
}

void BluetoothBms::onWebScanComplete(BLEScanResults scanResults) {
    (void)scanResults;
    bluetoothBms.completeWebScan();
}

bool BluetoothBms::tryStoreScanResult(const String& mac, const String& name, int rssi, uint8_t detectedType) {
    for (uint8_t i = 0; i < webScanResultCount_; i++) {
        if (webScanResults_[i].mac == mac) {
            if (rssi > webScanResults_[i].rssi) {
                webScanResults_[i].name = name;
                webScanResults_[i].rssi = rssi;
                webScanResults_[i].detectedType = detectedType;
            }
            return true;
        }
    }

    if (webScanResultCount_ >= MAX_WEB_SCAN_RESULTS) {
        return false;
    }

    webScanResults_[webScanResultCount_].mac = mac;
    webScanResults_[webScanResultCount_].name = name;
    webScanResults_[webScanResultCount_].rssi = rssi;
    webScanResults_[webScanResultCount_].detectedType = detectedType;
    webScanResultCount_++;
    return true;
}
