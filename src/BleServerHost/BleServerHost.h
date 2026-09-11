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

    BLEServer* getServer() const { return server_; }
    uint8_t getConnectedCount() const;

    // Advertising is suppressed while a BLE scan runs (see BluetoothBms).
    // The flag is authoritative: the connect/disconnect callbacks consult it
    // rather than starting advertising unconditionally.
    void setAdvertisingEnabled(bool enabled);
    bool isAdvertisingEnabled() const { return advertisingEnabled_; }

    // Re-starts advertising if, and only if, it is currently meant to be on.
    // Called from the server callbacks.
    void refreshAdvertising();

private:
    BLEServer* server_ = nullptr;
    bool advertisingEnabled_ = false;
};

#endif // BLE_SERVER_HOST_H
