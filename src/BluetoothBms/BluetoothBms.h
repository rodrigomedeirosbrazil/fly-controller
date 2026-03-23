#ifndef BLUETOOTH_BMS_H
#define BLUETOOTH_BMS_H

#include <Arduino.h>
#include <stdint.h>

class BLEScanResults;

enum BluetoothBmsScanStatus : uint8_t {
    BluetoothBmsScanIdle = 0,
    BluetoothBmsScanScanning = 1,
    BluetoothBmsScanComplete = 2,
    BluetoothBmsScanError = 3
};

struct BluetoothBmsScanResult {
    String mac;
    String name;
    int rssi;
    uint8_t detectedType;
    String advertisedServices;
};

class BluetoothBms {
public:
    void init();
    void update();

    bool isConnected() const;
    bool hasData() const;
    bool hasCellData() const;

    uint32_t getPackVoltageMilliVolts() const;
    int32_t getPackCurrentMilliAmps() const;
    uint8_t getSoCPercent() const;
    uint8_t getCellCount() const;
    uint8_t getTempCount() const;
    int16_t getTempCelsius(uint8_t index) const;

    uint16_t getCellVoltageMilliVolts(uint8_t index) const;
    uint16_t getCellMinMilliVolts() const;
    uint16_t getCellMaxMilliVolts() const;
    uint16_t getCellDeltaMilliVolts() const;

    bool startWebScan();
    void clearWebScanResults();
    uint8_t getWebScanStatus() const;
    const char* getWebScanError() const;
    uint8_t getWebScanResultCount() const;
    const BluetoothBmsScanResult* getWebScanResults() const;
    bool isWebScanBusy() const;
    uint8_t detectBmsTypeByMac(const String& macAddress);

private:
    static const uint8_t MAX_WEB_SCAN_RESULTS = 16;

    uint8_t getActiveType() const;
    void resetWebScanState(uint8_t status);
    void pauseTelemetryAdvertisingForScan();
    void resumeTelemetryAdvertisingAfterScan();
    void completeWebScan();
    static void onWebScanComplete(BLEScanResults scanResults);
    bool tryStoreScanResult(const String& mac, const String& name, int rssi, uint8_t detectedType, const String& advertisedServices);
    bool isValidMacAddress(const String& macAddress) const;

    uint8_t webScanStatus_ = BluetoothBmsScanIdle;
    char webScanError_[64] = {0};
    BluetoothBmsScanResult webScanResults_[MAX_WEB_SCAN_RESULTS];
    uint8_t webScanResultCount_ = 0;
    bool telemetryAdvertisingPausedForScan_ = false;
};

#endif // BLUETOOTH_BMS_H
