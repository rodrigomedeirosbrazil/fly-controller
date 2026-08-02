#include "TmotorTelemetry.h"
#include "../config_controller.h"
#if IS_TMOTOR
#include "../config.h"

extern TmotorCan tmotorCan;
extern Temperature motorTemp;
extern BatteryVoltageSensor batterySensor;

static int32_t motorTempMilliCelsiusForDisplay() {
    if (settings.getMotorTempSource() == MotorTempSourceAds1115) {
        return (int32_t)(motorTemp.getTemperature() * 1000.0);
    }
    // CAN (default): prefer CAN if recent, fall back to ADS1115
    if (tmotorCan.hasRecentMotorTempFromCan()) {
        return (int32_t)tmotorCan.getMotorTemperature() * 1000;
    }
    return (int32_t)(motorTemp.getTemperature() * 1000.0);
}

// Redundant sources: CAN (Status 5 / PUSHCAN), freshness + plausibility, or
// the NTC fallback. Invalid only when BOTH have failed — see
// docs/superpowers/specs/2026-08-01-signal-validity-design.md, "Detection
// per source". tmotorCan.getMotorTemperature() is a uint8_t Celsius value
// (unsigned, so it can't represent a too-cold reading — only the hot
// backstop applies here), compared against the existing millicelsius-domain
// MOTOR_TEMP_MAX_VALID converted to whole Celsius.
static SignalState motorTempStateForDisplay() {
    bool canOk = tmotorCan.hasRecentMotorTempFromCan() &&
                 tmotorCan.getMotorTemperature() <= (MOTOR_TEMP_MAX_VALID / 1000);
    if (canOk || motorTemp.isValid()) return SignalState::Valid;
    return SignalState::Invalid;
}

// CAN-only (no NTC on the ESC for Tmotor): stale when no fresh ESC_STATUS
// frame at all, invalid when the frame is fresh but the value is
// implausible.
static SignalState escTempStateForDisplay() {
    if (!tmotorCan.hasTelemetry()) return SignalState::Stale;
    if (tmotorCan.getEscTemperature() > (ESC_TEMP_MAX_VALID / 1000)) return SignalState::Invalid;
    return SignalState::Valid;
}

void TmotorTelemetry::update() {
    cachedBatteryVoltageMilliVolts = batterySensor.getVoltageMilliVolts();

    if (tmotorCan.hasTelemetry()) {
        cachedBatteryCurrentMilliAmps = tmotorCan.getBatteryCurrent();
        cachedRpm = tmotorCan.getRpm();
        cachedMotorTempMilliCelsius = motorTempMilliCelsiusForDisplay();
        cachedEscTempMilliCelsius = (int32_t)tmotorCan.getEscTemperature() * 1000;
    } else {
        // No ESC_STATUS recently: still expose battery (ADS1115), motor temp (CAN motor temp if fresh, else NTC)
        cachedBatteryCurrentMilliAmps = 0;
        cachedRpm = 0;
        cachedMotorTempMilliCelsius = motorTempMilliCelsiusForDisplay();
        cachedEscTempMilliCelsius = 0;
    }
    // Reflects real CAN freshness now, not a hardcoded true — a stale link
    // no longer masquerades as "has data" (see the design doc's problem #3).
    cachedHasData = tmotorCan.hasTelemetry();
    cachedLastUpdate = millis();

    cachedMotorTempState = motorTempStateForDisplay();
    cachedEscTempState = escTempStateForDisplay();
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
SignalState TmotorTelemetry::getBatteryVoltageState() const {
    return batterySensor.isValid() ? SignalState::Valid : SignalState::Invalid;
}
#endif
