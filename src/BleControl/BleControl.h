#ifndef BLE_CONTROL_H
#define BLE_CONTROL_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "ControlProtocol.h"
#include "DfuSession.h"

// The Fly Control GATT service: the app's own protocol, independent of the
// $XCTOD sentence that Xctod keeps broadcasting for XCTrack.
//
// Not advertised. The 31-byte advertising payload cannot carry a second
// 128-bit UUID, and the app discovers this service after connecting --
// which makes its presence the capability handshake with older firmware.
class BleControl {
public:
    BleControl();

    void init();
    void handle();

    // Called from the CMD characteristic's write callback, on the Bluedroid
    // task. Enqueues only -- never touches controller state. connId comes
    // from the GATT write event and says which central sent this.
    void enqueueFromCallback(const uint8_t* data, size_t len, uint16_t connId);

    // Called from the DFU characteristic's write-without-response callback,
    // on the Bluedroid task. Parses [offset u32 LE][data...] and stages the
    // bytes; the loop task writes them to flash. Never blocks -- a full stage
    // drops the packet, and the client restarts from the accepted offset.
    void enqueueDfuData(const uint8_t* data, size_t len);

    // Auth belongs to ONE connection, keyed by its conn_id -- not to the
    // service. Two centrals can be connected at once (XCTrack alongside the
    // app), and a single shared flag would let either one's AUTH unlock the
    // other. Clears only when the authenticated central is the one that left,
    // so another central's disconnect -- or a BLE client-role disconnect that
    // reaches this callback -- cannot drop a live session.
    void onCentralDisconnected(uint16_t connId) {
        if (connId == authConnId_) {
            authConnId_ = CONTROL_NO_CONN_ID;
        }
    }

private:
    static const unsigned long TELEMETRY_INTERVAL_MS = 1000;

    BLEService*        service_      = nullptr;
    BLECharacteristic* infoChar_     = nullptr;
    BLECharacteristic* telemetryChar_ = nullptr;
    BLECharacteristic* cmdChar_      = nullptr;
    BLECharacteristic* rspChar_      = nullptr;
    BLECharacteristic* dfuChar_      = nullptr;

    unsigned long lastTelemetryMs_ = 0;

    ControlRequestQueue queue_;
    uint16_t authConnId_ = CONTROL_NO_CONN_ID;

    uint32_t beepWatermark_ = 0;

    DfuSession dfu_;
    // Commit answers Ok and reboots on a LATER tick: the notification is
    // queued by the BLE stack, not flushed synchronously, so restarting in
    // the handler throws it away and the app reports a failure on an update
    // that worked.
    bool dfuRebootPending_ = false;

    void drainQueue();
    void notifyNewBeeps();
    void dispatch(const QueuedRequest& req);
    void respond(uint8_t op, uint8_t seq, ControlStatus status,
                 const uint8_t* payload, uint8_t len);

    ControlStatus handleAuth(const QueuedRequest& req);
    ControlStatus handleCfgGet(const QueuedRequest& req, uint8_t* out, uint8_t& outLen);
    ControlStatus handleCfgSet(const QueuedRequest& req);
    ControlStatus handleAction(const QueuedRequest& req, uint8_t* out, uint8_t& outLen);
    ControlStatus handleDfu(const QueuedRequest& req, uint8_t* out, uint8_t& outLen);
    uint16_t      dfuChunkSize() const;
    void          serviceDfu();
    bool seedCurrentConfig(const QueuedRequest& req, void* dst, size_t dstSize);

    void writeInfo();
    void notifyTelemetry();
    void fillTelemetry(ControlTelemetry& t) const;
};

#endif // BLE_CONTROL_H
