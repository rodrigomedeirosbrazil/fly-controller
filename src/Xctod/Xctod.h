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
class BatteryMonitor;

class Xctod {
public:
    Xctod();

    void init();
    void write();

private:
    unsigned long lastUpdate;
    static const unsigned long UPDATE_INTERVAL = 1000;

    BLEServer *pServer;
    BLEService *pService;
    BLECharacteristic *pCharacteristic;

    void writeBatteryInfo(String &data);
    void writeThrottleInfo(String &data);
    void writeMotorInfo(String &data);
    void writeEscInfo(String &data);
    void writeSystemStatus(String &data);
};

#endif // XCTOD_H
