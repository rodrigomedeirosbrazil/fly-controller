#include "TmotorTelemetry.h"
#include "../config_controller.h"
#if IS_TMOTOR
#include "../config.h"

extern TmotorCan tmotorCan;
extern Temperature motorTemp;

void TmotorTelemetry::update() {
    if (!tmotorCan.hasTelemetry()) {
        cachedHasData = false;
        return;
    }

    cachedBatteryVoltageMilliVolts = tmotorCan.getBatteryVoltageMilliVolts();
    cachedBatteryCurrentMilliAmps = tmotorCan.getBatteryCurrent();
    cachedRpm = tmotorCan.getRpm();
    cachedMotorTempMilliCelsius = (int32_t)(motorTemp.getTemperature() * 1000.0);
    cachedEscTempMilliCelsius = (int32_t)tmotorCan.getEscTemperature() * 1000;
    cachedLastUpdate = millis();
    cachedHasData = true;
}

bool TmotorTelemetry::hasData() const { return cachedHasData; }
uint16_t TmotorTelemetry::getBatteryVoltageMilliVolts() const { return cachedBatteryVoltageMilliVolts; }
uint32_t TmotorTelemetry::getBatteryCurrentMilliAmps() const { return cachedBatteryCurrentMilliAmps; }
uint16_t TmotorTelemetry::getRpm() const { return cachedRpm; }
int32_t TmotorTelemetry::getMotorTempMilliCelsius() const { return cachedMotorTempMilliCelsius; }
int32_t TmotorTelemetry::getEscTempMilliCelsius() const { return cachedEscTempMilliCelsius; }
unsigned long TmotorTelemetry::getLastUpdate() const { return cachedLastUpdate; }
#endif
