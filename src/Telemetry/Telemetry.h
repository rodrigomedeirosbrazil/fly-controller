#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Unified telemetry facade: delegates to HobbywingTelemetry, TmotorTelemetry, or XagTelemetry.
 * Provides stable getter API for Power, BatteryMonitor, Xctod.
 */
class Telemetry {
public:
    void init();
    void update();

    bool hasData() const;
    uint16_t getBatteryVoltageMilliVolts() const;
    uint32_t getBatteryCurrentMilliAmps() const;
    uint16_t getRpm() const;
    int32_t getMotorTempMilliCelsius() const;
    int32_t getEscTempMilliCelsius() const;
    unsigned long getLastUpdate() const;
};

extern Telemetry telemetry;

#endif // TELEMETRY_H
