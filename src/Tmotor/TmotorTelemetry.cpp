#include "TmotorTelemetry.h"
#include "../config_controller.h"
#if IS_TMOTOR
#include "../config.h"

extern TmotorCan tmotorCan;
extern Temperature motorTemp;

void TmotorTelemetry::update() {
    if (tmotorCan.hasTelemetry()) {
        cachedBatteryVoltageMilliVolts = tmotorCan.getBatteryVoltageMilliVolts();
        cachedBatteryCurrentMilliAmps = tmotorCan.getBatteryCurrent();
        cachedRpm = tmotorCan.getRpm();
        cachedMotorTempMilliCelsius = (int32_t)(motorTemp.getTemperature() * 1000.0);
        cachedEscTempMilliCelsius = (int32_t)tmotorCan.getEscTemperature() * 1000;
        cachedLastUpdate = millis();
        cachedHasData = true;
    } else {
        // No CAN telemetry: still expose data that does not depend on CAN (motor temp from NTC/ADS1115)
        cachedBatteryVoltageMilliVolts = 0;
        cachedBatteryCurrentMilliAmps = 0;
        cachedRpm = 0;
        cachedMotorTempMilliCelsius = (int32_t)(motorTemp.getTemperature() * 1000.0);
        cachedEscTempMilliCelsius = 0;
        cachedLastUpdate = millis();
        cachedHasData = true;
    }
}

bool TmotorTelemetry::hasData() const { return cachedHasData; }
uint16_t TmotorTelemetry::getBatteryVoltageMilliVolts() const { return cachedBatteryVoltageMilliVolts; }
uint32_t TmotorTelemetry::getBatteryCurrentMilliAmps() const { return cachedBatteryCurrentMilliAmps; }
uint16_t TmotorTelemetry::getRpm() const { return cachedRpm; }
int32_t TmotorTelemetry::getMotorTempMilliCelsius() const { return cachedMotorTempMilliCelsius; }
int32_t TmotorTelemetry::getEscTempMilliCelsius() const { return cachedEscTempMilliCelsius; }
unsigned long TmotorTelemetry::getLastUpdate() const { return cachedLastUpdate; }
#endif
