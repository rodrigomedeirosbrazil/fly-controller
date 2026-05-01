#include "TmotorTelemetry.h"
#include "TmotorCan.h"
#include "../config_controller.h"
#if IS_TMOTOR

extern TmotorCan tmotorCan;

void TmotorTelemetry::update()
{
    cachedBatteryVoltageMilliVolts = tmotorCan.getBatteryVoltageMilliVolts();
    cachedBatteryCurrentMilliAmps = tmotorCan.getBatteryCurrent();
    cachedRpm = tmotorCan.getRpm();
    cachedMotorTempMilliCelsius = (int32_t)tmotorCan.getMotorTemperature() * 1000;
    cachedEscTempMilliCelsius = (int32_t)tmotorCan.getEscTemperature() * 1000;
    cachedLastUpdate = millis();
    cachedHasData = tmotorCan.hasTelemetry();
}

bool TmotorTelemetry::hasData() const { return cachedHasData; }
uint16_t TmotorTelemetry::getBatteryVoltageMilliVolts() const { return cachedBatteryVoltageMilliVolts; }
uint32_t TmotorTelemetry::getBatteryCurrentMilliAmps() const { return cachedBatteryCurrentMilliAmps; }
uint16_t TmotorTelemetry::getRpm() const { return cachedRpm; }
int32_t TmotorTelemetry::getMotorTempMilliCelsius() const { return cachedMotorTempMilliCelsius; }
int32_t TmotorTelemetry::getEscTempMilliCelsius() const { return cachedEscTempMilliCelsius; }
unsigned long TmotorTelemetry::getLastUpdate() const { return cachedLastUpdate; }

#endif
