#ifndef TELEMETRY_DATA_H
#define TELEMETRY_DATA_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Unified telemetry data structure (pure data, no logic)
 * All values use integer types with 3 decimal places precision to avoid float overhead
 */
struct TelemetryData {
    // Status
    bool hasTelemetry;

    // Battery
    uint16_t batteryVoltageMilliVolts;  // mV (ex: 44100 = 44.100V, max 60V = 60000 mV)
    uint32_t batteryCurrentMilliAmps;  // mA (ex: 50000 = 50.000A, max 500A = 500000 mA)

    // Motor
    uint16_t rpm;
    int32_t motorTemperatureMilliCelsius;  // m°C (ex: 50000 = 50.000°C)

    // ESC
    int32_t escTemperatureMilliCelsius;    // m°C (ex: 80000 = 80.000°C)

    // Timestamp da última atualização
    unsigned long lastUpdate;
};

#endif // TELEMETRY_DATA_H

