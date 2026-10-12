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
    // APPEND-ONLY: this service is the XCTrack compatibility surface and
    // nothing else. Fields 1-16 of the $XCTOD sentence never move or change
    // meaning; new fields go at the end only (field 17 is remaining_min). It
    // has two positional consumers (XCTrack's .xcfg and fly-app's
    // xctod_parser.dart) and no version handshake. Everything richer goes in
    // BleControl.
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
    void writeRemainingTime(char* data, size_t size, size_t& used);
};

#endif // XCTOD_H
