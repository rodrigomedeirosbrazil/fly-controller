#include "XagTelemetry.h"
#include "../config_controller.h"
#if IS_XAG
#include "../config.h"

extern Temperature motorTemp;
extern Temperature escTemp;
extern BatteryVoltageSensor batterySensor;

void XagTelemetry::update() {
    cachedBatteryVoltageMilliVolts = batterySensor.getVoltageMilliVolts();
    cachedMotorTempMilliCelsius = (int32_t)(motorTemp.getTemperature() * 1000.0);
    cachedEscTempMilliCelsius = (int32_t)(escTemp.getTemperature() * 1000.0);
    cachedLastUpdate = millis();
    cachedHasData = true;
}

bool XagTelemetry::hasData() const { return cachedHasData; }
uint16_t XagTelemetry::getBatteryVoltageMilliVolts() const { return cachedBatteryVoltageMilliVolts; }
int32_t XagTelemetry::getMotorTempMilliCelsius() const { return cachedMotorTempMilliCelsius; }
int32_t XagTelemetry::getEscTempMilliCelsius() const { return cachedEscTempMilliCelsius; }
unsigned long XagTelemetry::getLastUpdate() const { return cachedLastUpdate; }
#endif
