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

    // Registers the Nordic UART service on the shared BLE server. The radio
    // itself belongs to BleServerHost.
    //
    // FROZEN: this service is the XCTrack compatibility surface and nothing
    // else. No field is ever added to the $XCTOD sentence — it has two
    // positional consumers (XCTrack's .xcfg and fly-app's xctod_parser.dart)
    // and no version handshake. Everything new goes in BleControl.
    void init();
    void write();

private:
    unsigned long lastUpdate;
    static const unsigned long UPDATE_INTERVAL = 1000;
    static const size_t TELEMETRY_BUFFER_SIZE = 256;

    BLEService *pService;
    BLECharacteristic *pCharacteristic;

    void writeBatteryInfo(char* data, size_t size, size_t& used);
    void writeThrottleInfo(char* data, size_t size, size_t& used);
    void writeMotorInfo(char* data, size_t size, size_t& used);
    void writeEscInfo(char* data, size_t size, size_t& used);
    void writeSystemStatus(char* data, size_t size, size_t& used);
    void writeBmsInfo(char* data, size_t size, size_t& used);
};

#endif // XCTOD_H
