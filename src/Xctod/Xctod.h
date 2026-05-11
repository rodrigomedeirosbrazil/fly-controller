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
    void setAdvertisingEnabled(bool enabled);
    bool isAdvertisingEnabled() const;

private:
    unsigned long lastUpdate;
    static const unsigned long UPDATE_INTERVAL = 1000;
    static const size_t TELEMETRY_BUFFER_SIZE = 256;
    bool advertisingEnabled;

    BLEServer *pServer;
    BLEService *pService;
    BLECharacteristic *pCharacteristic;

    void writeBatteryInfo(char* data, size_t size, size_t& used);
    void writeThrottleInfo(char* data, size_t size, size_t& used);
    void writeMotorInfo(char* data, size_t size, size_t& used);
    void writeEscInfo(char* data, size_t size, size_t& used);
    void writeSystemStatus(char* data, size_t size, size_t& used);
    void writeBmsInfo(char* data, size_t size, size_t& used);
    void writeMotorTempNtc(char* data, size_t size, size_t& used);
};

#endif // XCTOD_H
