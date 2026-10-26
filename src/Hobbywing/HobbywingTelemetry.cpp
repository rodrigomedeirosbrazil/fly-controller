#include "HobbywingTelemetry.h"
#include "../config_controller.h"
#if IS_HOBBYWING
#include "../config.h"
#include "HobbywingCanParser.h"

void HobbywingTelemetry::update() {
    cachedBatteryVoltageMilliVolts = batterySensor.getVoltageMilliVolts();
    cachedBatteryVoltageState = batterySensor.isValid() ? SignalState::Valid : SignalState::Invalid;

    cachedMotorTempMilliCelsius = motorTemp.getTemperature() * 1000;
    cachedMotorTempState = motorTemp.isValid() ? SignalState::Valid : SignalState::Invalid;

    const bool hasStatus = hobbywingCan.hasTelemetry();
    if (hasStatus) {
        cachedBatteryCurrentMilliAmps = hobbywingCan.getBatteryCurrentMilliAmps();
        cachedEscTempMilliCelsius = (int32_t)hobbywingCan.getEscTemperature() * 1000;
    } else {
        cachedBatteryCurrentMilliAmps = 0;
        cachedEscTempMilliCelsius = 0;
    }
    cachedRpm = hobbywingCan.hasRpm() ? hobbywingCan.getRpm() : 0;
    cachedEscTempState = HobbywingCanParser::classifyEscTemp(
        hasStatus, hobbywingCan.getEscTemperature(), ESC_TEMP_MAX_VALID / 1000);

    cachedHasData = hasStatus;
    cachedLastUpdate = millis();
}

bool HobbywingTelemetry::hasData() const { return cachedHasData; }
uint16_t HobbywingTelemetry::getBatteryVoltageMilliVolts() const { return cachedBatteryVoltageMilliVolts; }
uint32_t HobbywingTelemetry::getBatteryCurrentMilliAmps() const { return cachedBatteryCurrentMilliAmps; }
uint16_t HobbywingTelemetry::getRpm() const { return cachedRpm; }
int32_t HobbywingTelemetry::getMotorTempMilliCelsius() const { return cachedMotorTempMilliCelsius; }
int32_t HobbywingTelemetry::getEscTempMilliCelsius() const { return cachedEscTempMilliCelsius; }
unsigned long HobbywingTelemetry::getLastUpdate() const { return cachedLastUpdate; }
SignalState HobbywingTelemetry::getMotorTempState() const { return cachedMotorTempState; }
SignalState HobbywingTelemetry::getEscTempState() const { return cachedEscTempState; }
SignalState HobbywingTelemetry::getBatteryVoltageState() const { return cachedBatteryVoltageState; }
#endif
