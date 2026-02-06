#ifndef XCTOD_H
#define XCTOD_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

class Throttle;
class Temperature;
class Power;

class Xctod {
public:
    Xctod();

    void init();
    void write();

private:
    unsigned long lastUpdate;
    static const unsigned long UPDATE_INTERVAL = 1000;

    // Coulomb Counting state (using milliampere-hours to avoid float)
    uint32_t batteryCapacityMilliAh;  // Capacity in mAh (e.g., 65000 = 65.0 Ah)
    uint32_t remainingMilliAh;        // Remaining capacity in mAh
    unsigned long lastCoulombTs;

    BLEServer *pServer;
    BLEService *pService;
    BLECharacteristic *pCharacteristic;

    void updateCoulombCount();
    void recalibrateFromVoltage();
    void writeBatteryInfo(String &data);
    void writeThrottleInfo(String &data);
    void writeMotorInfo(String &data);
    void writeEscInfo(String &data);
    void writeSystemStatus(String &data);
};

#endif // XCTOD_H
