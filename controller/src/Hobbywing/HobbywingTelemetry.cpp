#include "HobbywingTelemetry.h"
#include "../config_controller.h"
#if IS_HOBBYWING
#include "../config.h"

extern HobbywingCan hobbywingCan;
extern Temperature motorTemp;

void HobbywingTelemetry::update() {
    if (hobbywingCan.hasTelemetry()) {
        cachedBatteryVoltageMilliVolts = hobbywingCan.getBatteryVoltageMilliVolts();
        cachedBatteryCurrentMilliAmps = hobbywingCan.getBatteryCurrentMilliAmps();
        cachedRpm = hobbywingCan.getRpm();
        cachedMotorTempMilliCelsius = (int32_t)(motorTemp.getTemperature() * 1000.0);
        cachedEscTempMilliCelsius = (int32_t)hobbywingCan.getEscTemperature() * 1000;
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

bool HobbywingTelemetry::hasData() const { return cachedHasData; }
uint16_t HobbywingTelemetry::getBatteryVoltageMilliVolts() const { return cachedBatteryVoltageMilliVolts; }
uint32_t HobbywingTelemetry::getBatteryCurrentMilliAmps() const { return cachedBatteryCurrentMilliAmps; }
uint16_t HobbywingTelemetry::getRpm() const { return cachedRpm; }
int32_t HobbywingTelemetry::getMotorTempMilliCelsius() const { return cachedMotorTempMilliCelsius; }
int32_t HobbywingTelemetry::getEscTempMilliCelsius() const { return cachedEscTempMilliCelsius; }
unsigned long HobbywingTelemetry::getLastUpdate() const { return cachedLastUpdate; }
#endif
