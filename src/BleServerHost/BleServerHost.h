#ifndef BLE_SERVER_HOST_H
#define BLE_SERVER_HOST_H

#include <BLEDevice.h>
#include <BLEServer.h>

// Owns the BLE radio: device init, TX power caps, the BLEServer, and the
// advertising policy. Xctod and BleControl each register a service on the
// server this class exposes; neither touches BLEDevice directly, so there is
// exactly one place that decides when the controller is discoverable.
class BleServerHost {
public:
    void init(const char* deviceName);

    // Reconciles the radio with the requested advertising state. Called from
    // loop(). See onConnectionChanged() for why this exists.
    void handle();

    BLEServer* getServer() const { return server_; }

    // Advertising is suppressed while a BLE scan runs (see BluetoothBms).
    // Called from the loop task only.
    void setAdvertisingEnabled(bool enabled);

    // Called from the server callbacks, on the Bluedroid task. Raises a flag
    // and nothing else.
    //
    // Bluedroid stops advertising when a central connects, so it has to be
    // re-started or a second central (XCTrack alongside the fly-app) can
    // never discover the controller. Doing that from the callback would race
    // BluetoothBms' scan pause: the callback could read advertisingEnabled_
    // as true, be preempted while the loop task sets it false and calls
    // stopAdvertising(), then resume and switch advertising back on in the
    // middle of a BMS scan. Deferring to handle() keeps every
    // start/stopAdvertising call on one task, so the race cannot exist.
    void onConnectionChanged() { connectionChanged_ = true; }

private:
    BLEServer* server_ = nullptr;
    bool advertisingEnabled_ = false;
    volatile bool connectionChanged_ = false;
};

#endif // BLE_SERVER_HOST_H
