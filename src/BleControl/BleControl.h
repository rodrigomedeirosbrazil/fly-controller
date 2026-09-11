#ifndef BLE_CONTROL_H
#define BLE_CONTROL_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "ControlProtocol.h"

// The Fly Control GATT service: the app's own protocol, independent of the
// $XCTOD sentence that Xctod keeps broadcasting for XCTrack.
//
// Not advertised. The 31-byte advertising payload cannot carry a second
// 128-bit UUID, and the app discovers this service after connecting --
// which makes its presence the capability handshake with older firmware.
class BleControl {
public:
    void init();
    void handle();

private:
    static const unsigned long TELEMETRY_INTERVAL_MS = 1000;

    BLEService*        service_      = nullptr;
    BLECharacteristic* infoChar_     = nullptr;
    BLECharacteristic* telemetryChar_ = nullptr;
    BLECharacteristic* cmdChar_      = nullptr;
    BLECharacteristic* rspChar_      = nullptr;

    unsigned long lastTelemetryMs_ = 0;

    void writeInfo();
    void notifyTelemetry();
    void fillTelemetry(ControlTelemetry& t) const;
};

#endif // BLE_CONTROL_H
