#ifndef TELEMETRY_DATA_H
#define TELEMETRY_DATA_H

#include <Arduino.h>

/**
 * Unified telemetry data structure
 * All values use integer types with 3 decimal places precision to avoid float overhead
 */
struct TelemetryData {
    // Status
    bool isReady;

    // Battery
    uint32_t batteryVoltageMilliVolts;  // mV (ex: 44100 = 44.100V)
    uint32_t batteryCurrentMilliAmps;    // mA (ex: 5000 = 5.000A)

    // Motor
    uint16_t rpm;
    int32_t motorTemperatureMilliCelsius;  // m°C (ex: 50000 = 50.000°C)

    // ESC
    int32_t escTemperatureMilliCelsius;    // m°C (ex: 80000 = 80.000°C)

    // Timestamp da última atualização
    unsigned long lastUpdate;
};

/**
 * Conversion functions for display purposes only
 * These return float only when necessary for formatting (e.g., in Xctod)
 */
inline float milliVoltsToVolts(uint32_t mV) {
    return mV / 1000.0f;
}

inline float milliAmpsToAmps(uint32_t mA) {
    return mA / 1000.0f;
}

inline float milliCelsiusToCelsius(int32_t mC) {
    return mC / 1000.0f;
}

#endif // TELEMETRY_DATA_H

