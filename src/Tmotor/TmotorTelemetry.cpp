#include "TmotorTelemetry.h"
#include "../config_controller.h"
#if IS_TMOTOR
#include "../config.h"

extern TmotorCan tmotorCan;
extern Temperature motorTemp;
extern BatteryVoltageSensor batterySensor;

struct MotorTempReading {
    int32_t milliCelsius;
    SignalState state;
};

// Redundant sources: CAN (Status 5 / PUSHCAN) when fresh, plausible, and not
// overridden by settings, or the NTC fallback. The state always reflects
// whichever source actually produced the returned value — never a
// different source's health than the one backing the number — see
// docs/superpowers/specs/2026-08-01-signal-validity-design.md.
//
// Known gap, not fixed here: TmotorCan clamps/truncates a corrupt CAN
// temperature into its uint8_t storage range before this function ever
// sees it, which can hide a garbage reading that happens to land inside
// the plausible range after clamping. The `<= (MOTOR_TEMP_MAX_VALID/1000)`
// check below only catches the coarse out-of-range cases (including a 255
// fault sentinel some ESC firmware uses); a subtler corruption at the
// TmotorCan decode layer is not currently detectable here.
static MotorTempReading readMotorTemp() {
    const bool forceNtc = settings.getMotorTempSource() == MotorTempSourceAds1115;
    const bool canOk = !forceNtc &&
                        tmotorCan.hasRecentMotorTempFromCan() &&
                        tmotorCan.getMotorTemperature() <= (MOTOR_TEMP_MAX_VALID / 1000);
    if (canOk) {
        return { (int32_t)tmotorCan.getMotorTemperature() * 1000, SignalState::Valid };
    }
    return {
        (int32_t)(motorTemp.getTemperature() * 1000.0),
        motorTemp.isValid() ? SignalState::Valid : SignalState::Invalid
    };
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
#endif
