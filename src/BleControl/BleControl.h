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

    // Called from the CMD characteristic's write callback, on the Bluedroid
    // task. Enqueues only -- never touches controller state.
    void enqueueFromCallback(const uint8_t* data, size_t len);

    // Auth is per connection. BleServerHost's disconnect path calls this, so
    // a reconnecting central starts locked.
    void onCentralDisconnected() { authenticated_ = false; }

private:
    static const unsigned long TELEMETRY_INTERVAL_MS = 1000;

    BLEService*        service_      = nullptr;
    BLECharacteristic* infoChar_     = nullptr;
    BLECharacteristic* telemetryChar_ = nullptr;
    BLECharacteristic* cmdChar_      = nullptr;
    BLECharacteristic* rspChar_      = nullptr;

    unsigned long lastTelemetryMs_ = 0;

    ControlRequestQueue queue_;
    bool authenticated_ = false;

    void drainQueue();
    void dispatch(const QueuedRequest& req);
    void respond(uint8_t op, uint8_t seq, ControlStatus status,
                 const uint8_t* payload, uint8_t len);

    ControlStatus handleAuth(const QueuedRequest& req);
    ControlStatus handleCfgGet(const QueuedRequest& req, uint8_t* out, uint8_t& outLen);
    ControlStatus handleCfgSet(const QueuedRequest& req);
    ControlStatus handleAction(const QueuedRequest& req, uint8_t* out, uint8_t& outLen);
    bool seedCurrentConfig(const QueuedRequest& req, void* dst, size_t dstSize);

    void writeInfo();
    void notifyTelemetry();
    void fillTelemetry(ControlTelemetry& t) const;
};

#endif // BLE_CONTROL_H
