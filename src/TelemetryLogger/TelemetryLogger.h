#ifndef TELEMETRY_LOGGER_H
#define TELEMETRY_LOGGER_H

#include <Arduino.h>

class TelemetryLogger {
public:
    TelemetryLogger();
    void init();
    void handle();

private:
    unsigned long lastUpdate;
    static const unsigned long UPDATE_INTERVAL = 1000;
    static const size_t LINE_BUFFER_SIZE = 224;

    void writeBatteryInfo(char* data, size_t size, size_t& used);
    void writeThrottleInfo(char* data, size_t size, size_t& used);
    void writeMotorInfo(char* data, size_t size, size_t& used);
    void writeEscInfo(char* data, size_t size, size_t& used);
    void writeBmsInfo(char* data, size_t size, size_t& used);
};

#endif
