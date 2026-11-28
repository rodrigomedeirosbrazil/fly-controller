#include "Power.h"
#include "../config.h"
#include "../BoardConfig.h"
#include "../DisarmReason.h"
#include "../Throttle/Throttle.h"

extern Throttle throttle;
extern Settings settings;

Power::Power() {
    lastPowerCalculationTime = 0;
    power = 100;
    batteryPowerFloor = 100;
    activeLimitCauses_ = POWER_LIMIT_NONE;
    disarmScale_ = 100;
}

void Power::onArmed() {
    batteryContract_.onArmed();
}

void Power::checkSignalLoss() {
    // Only advance the contract while armed. It carries timing state (the
    // loss debounce), and letting it run while disarmed would leave a stale
    // in-progress invalid episode to be re-measured against the next
    // session. onArmed() re-opens it on the next arm anyway.
    if (!throttle.isArmed()) {
        return;
    }

    if (batteryContract_.update(telemetry.isBatteryVoltageValid(), millis(), SIGNAL_LOSS_GRACE_MS)) {
        throttle.setDisarmed(DisarmReason::BatteryVoltageLost);
    }
}

// The motor-temp reading the limiter trusts comes from the CONFIGURED source
// only. On Tmotor the telemetry falls back from CAN to the NTC when CAN goes
// stale, but the two sensors sit in different places and read differently,
// so the fallback is shown to the pilot and never used for derating: CAN
// lost means motor-temp protection is suspended until CAN returns. XAG and
// Hobbywing have a single source and report origin None.
static MotorTempOrigin configuredMotorTempOrigin() {
#if IS_TMOTOR
    return settings.getMotorTempSource() == MotorTempSourceAds1115
        ? MotorTempOrigin::Ntc
        : MotorTempOrigin::Can;
#else
    return MotorTempOrigin::None;
#endif
}

bool Power::isMotorTempProtecting() const {
    return telemetry.isMotorTempValid()
        && telemetry.getMotorTempOrigin() == configuredMotorTempOrigin();
}

bool Power::isEscTempProtecting() const {
    return telemetry.isEscTempValid();
}

void Power::updateTempProtection() {
    const uint32_t now = millis();
    motorTempProtection_.update(isMotorTempProtecting(), now, TEMP_PROTECTION_DEBOUNCE_MS);
    escTempProtection_.update(isEscTempProtecting(), now, TEMP_PROTECTION_DEBOUNCE_MS);
}

uint8_t Power::getSuspendedTempProtections() const {
    if (!settings.getPowerControlEnabled()) return POWER_LIMIT_NONE;
    uint8_t mask = POWER_LIMIT_NONE;
    if (motorTempProtection_.isSuspended()) mask |= POWER_LIMIT_MOTOR_TEMP;
    if (escTempProtection_.isSuspended())   mask |= POWER_LIMIT_ESC_TEMP;
    return mask;
}

unsigned int Power::getPwm() {
    ThrottleOutputInput in{};
    in.nowMs          = millis();
    in.calibrated     = throttle.isCalibrated();
    in.armed          = throttle.isArmed();
    in.demandPpm      = throttle.getDemandPpm();
    in.ceilingPpm     = (uint32_t)getPower() * 10000u;
    in.disarmScalePct = disarmScale_;
    in.rpmFresh       = telemetry.isRpmFresh();
    in.rpm            = telemetry.getRpm();
    return outputToPwm(output_.update(in), ESC_MIN_PWM, ESC_MAX_PWM);
}

unsigned int Power::getPower() {
    if (millis() - lastPowerCalculationTime < 500) {
        return power;
    }
    lastPowerCalculationTime = millis();
    power = calcPower();
    return power;
}

unsigned int Power::calcPower() {
    // Note: disabling power control here only stops the percentage-based
    // limiting below — battery disarm-on-signal-loss is a separate safety
    // behavior handled by checkSignalLoss(), called independently every loop
    // tick, and is not affected by this setting.
    if (!settings.getPowerControlEnabled()) {
        activeLimitCauses_ = POWER_LIMIT_NONE;
        motorTempCeiling_.reset();
        escTempCeiling_.reset();
        return 100;
    }
    const uint32_t nowMs = millis();
    unsigned int batteryLimit   = calcBatteryLimit();
    unsigned int motorTempLimit = motorTempCeiling_.update(calcMotorTempLimit(), nowMs);
    unsigned int escTempLimit   = escTempCeiling_.update(calcEscTempLimit(), nowMs);

    uint8_t causes = POWER_LIMIT_NONE;
    if (batteryLimit < 100)   causes |= POWER_LIMIT_BATTERY;
    if (motorTempLimit < 100) causes |= POWER_LIMIT_MOTOR_TEMP;
    if (escTempLimit < 100)   causes |= POWER_LIMIT_ESC_TEMP;
    activeLimitCauses_ = causes;

    return min(min(batteryLimit, motorTempLimit), escTempLimit);
}

unsigned int Power::calcBatteryLimit() {
    if (!getBoardConfig().useBatteryLimit) return 100;

    if (!batteryContract_.shouldLimit(telemetry.isBatteryVoltageValid())) return 100;

    uint16_t batteryMilliVolts = telemetry.getBatteryVoltageMilliVolts();
    const unsigned int STEP_DECREASE = 5;

    if (batteryMilliVolts > settings.getBatteryMinVoltage()) {
        return batteryPowerFloor;
    }
    if (batteryPowerFloor < STEP_DECREASE) {
        batteryPowerFloor = 0;
        return 0;
    }
    batteryPowerFloor -= STEP_DECREASE;
    return batteryPowerFloor;
}

unsigned int Power::calcMotorTempLimit() {
    // The raw target is not debounced: it jumps to 100 on the very sample the
    // configured source goes bad. The applied ceiling is slewed by
    // motorTempCeiling_ (fast down, slow up), so it releases gradually. Only
    // the alarm (TempProtectionMonitor) waits out the debounce window.
    if (!isMotorTempProtecting()) return 100;

    int32_t motorTempMilliCelsius = telemetry.getMotorTempMilliCelsius();
    int32_t reductionStart = settings.getMotorTempReductionStart();
    int32_t maxTemp        = settings.getMotorMaxTemp();

    if (motorTempMilliCelsius < reductionStart) return 100;
    if (reductionStart == maxTemp) return 0;

    return constrain(map(motorTempMilliCelsius, reductionStart, maxTemp, 100, 0), 0, 100);
}

unsigned int Power::calcEscTempLimit() {
    if (!isEscTempProtecting()) return 100;

    int32_t escTempMilliCelsius = telemetry.getEscTempMilliCelsius();
    int32_t reductionStart = settings.getEscTempReductionStart();
    int32_t maxTemp        = settings.getEscMaxTemp();

    if (escTempMilliCelsius < reductionStart) return 100;
    if (reductionStart == maxTemp) return 0;

    return constrain(map(escTempMilliCelsius, reductionStart, maxTemp, 100, 0), 0, 100);
}

void Power::resetBatteryPowerFloor() {
    batteryPowerFloor = 100;
}
