#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdint.h>
#include <stdbool.h>
#include "SignalState.h"
#include "MotorTempOrigin.h"

/**
 * Unified telemetry facade: delegates to TmotorTelemetry or XagTelemetry.
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

    SignalState getMotorTempState() const;
    SignalState getEscTempState() const;
    SignalState getBatteryVoltageState() const;
    MotorTempOrigin getMotorTempOrigin() const;
    bool isMotorTempValid() const { return getMotorTempState() == SignalState::Valid; }
    bool isEscTempValid() const { return getEscTempState() == SignalState::Valid; }
    bool isBatteryVoltageValid() const { return getBatteryVoltageState() == SignalState::Valid; }
};

extern Telemetry telemetry;

#endif // TELEMETRY_H
