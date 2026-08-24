#ifndef TELEMETRY_BACKEND_H
#define TELEMETRY_BACKEND_H

#include <stdint.h>
#include <stdbool.h>
#include "SignalState.h"
#include "MotorTempOrigin.h"

/**
 * Telemetry backend interface: function pointers for runtime dispatch.
 * Eliminates repetitive #if IS_TMOTOR / #elif IS_XAG in Telemetry.cpp.
 */
struct TelemetryBackend {
    void (*update)(void);
    bool (*hasData)(void);
    uint16_t (*getBatteryVoltageMilliVolts)(void);
    uint32_t (*getBatteryCurrentMilliAmps)(void);
    uint16_t (*getRpm)(void);
    int32_t (*getMotorTempMilliCelsius)(void);
    int32_t (*getEscTempMilliCelsius)(void);
    unsigned long (*getLastUpdate)(void);
    SignalState (*getMotorTempState)(void);
    SignalState (*getEscTempState)(void);
    SignalState (*getBatteryVoltageState)(void);
    MotorTempOrigin (*getMotorTempOrigin)(void);
};

#endif // TELEMETRY_BACKEND_H
