#include "TmotorTelemetry.h"
#include "../config_controller.h"
#if IS_TMOTOR
#include "../config.h"
#include "MotorTempSourceLogic.h"

extern TmotorCan tmotorCan;
extern Temperature motorTemp;
extern BatteryVoltageSensor batterySensor;

// Plausibility is per sample inside MotorTempFilter: an implausible Status 5
// sample is dropped and only ages the reading out (stale after 1 s).
static MotorTempReading readMotorTemp() {
    const bool forceNtc = settings.getMotorTempSource() == MotorTempSourceAds1115;
    const bool canFreshAndPlausible = tmotorCan.hasRecentMotorTempFromCan();
    return selectMotorTempReading(
        forceNtc,
        canFreshAndPlausible,
        tmotorCan.getMotorTempMilliCelsius(),
        motorTemp.getTemperature(),
        motorTemp.isValid()
    );
}

// CAN-only (no NTC on the ESC for Tmotor): stale when no fresh ESC_STATUS
// frame at all, invalid when the frame is fresh but the value is
// implausible.
static SignalState escTempState() {
    if (!tmotorCan.hasTelemetry()) return SignalState::Stale;
    if (tmotorCan.getEscTemperature() > (ESC_TEMP_MAX_VALID / 1000)) return SignalState::Invalid;
    return SignalState::Valid;
}

void TmotorTelemetry::update() {
    cachedBatteryVoltageMilliVolts = batterySensor.getVoltageMilliVolts();

    MotorTempReading motorReading = readMotorTemp();
    cachedMotorTempMilliCelsius = motorReading.milliCelsius;
    cachedMotorTempState = motorReading.state;
    cachedMotorTempOrigin = motorReading.origin;

    if (tmotorCan.hasTelemetry()) {
        cachedBatteryCurrentMilliAmps = tmotorCan.getBatteryCurrent();
        cachedRpm = tmotorCan.getRpm();
        cachedEscTempMilliCelsius = (int32_t)tmotorCan.getEscTemperature() * 1000;
    } else {
        cachedBatteryCurrentMilliAmps = 0;
        cachedRpm = 0;
        cachedEscTempMilliCelsius = 0;
    }
    cachedHasData = tmotorCan.hasTelemetry();
    cachedLastUpdate = millis();

    cachedEscTempState = escTempState();
    cachedBatteryVoltageState = batterySensor.isValid() ? SignalState::Valid : SignalState::Invalid;
}

bool TmotorTelemetry::hasData() const { return cachedHasData; }
uint16_t TmotorTelemetry::getBatteryVoltageMilliVolts() const { return cachedBatteryVoltageMilliVolts; }
uint32_t TmotorTelemetry::getBatteryCurrentMilliAmps() const { return cachedBatteryCurrentMilliAmps; }
uint16_t TmotorTelemetry::getRpm() const { return cachedRpm; }
int32_t TmotorTelemetry::getMotorTempMilliCelsius() const { return cachedMotorTempMilliCelsius; }
int32_t TmotorTelemetry::getEscTempMilliCelsius() const { return cachedEscTempMilliCelsius; }
unsigned long TmotorTelemetry::getLastUpdate() const { return cachedLastUpdate; }
SignalState TmotorTelemetry::getMotorTempState() const { return cachedMotorTempState; }
SignalState TmotorTelemetry::getEscTempState() const { return cachedEscTempState; }
SignalState TmotorTelemetry::getBatteryVoltageState() const { return cachedBatteryVoltageState; }
MotorTempOrigin TmotorTelemetry::getMotorTempOrigin() const { return cachedMotorTempOrigin; }
#endif
