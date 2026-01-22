#ifndef XCTOD_H
#define XCTOD_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

class Throttle;
class Canbus;
#ifndef XAG
#ifdef T_MOTOR
class Tmotor;
#else
class Hobbywing;
#endif
#endif
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

    // Coulomb Counting state
    float batteryCapacityAh;
    float remainingAh;
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
