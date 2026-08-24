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
    startState = StartState::IDLE;
    startingBeganAt = 0;
    idleBeganAt = 0;
}

void Power::onArmed() {
    motorTempContract_.onArmed();
    escTempContract_.onArmed();
    batteryContract_.onArmed();
}

void Power::checkSignalLoss() {
    // Only advance the contracts while armed. They carry timing state now
    // (the loss debounce), and letting it run while disarmed would leave a
    // stale in-progress invalid episode to be re-measured against the next
    // session. onArmed() re-opens them on the next arm anyway.
    if (!throttle.isArmed()) {
        return;
    }

    const uint32_t now = millis();

    // All three contracts are updated unconditionally, even if an earlier
    // one in this same call already disarmed — every contract must see this
    // tick's sample to keep its own debounce timer honest. Only the first
    // one to fire actually latches a DisarmReason, since
    // Throttle::setDisarmed() early-returns once already disarmed.
    //
    // The motor-temp contract passes the reading's origin (Can/Ntc) as its
    // source tag, so a mid-flight switch of which sensor feeds the value is
    // treated as loss of the sensor the pilot armed with. ESC temp and
    // battery voltage have a single source and pass no tag.
    SignalArmContract::Outcome motorOutcome = motorTempContract_.update(
        telemetry.isMotorTempValid(), now, SIGNAL_LOSS_GRACE_MS,
        (uint8_t)telemetry.getMotorTempOrigin());
    SignalArmContract::Outcome escOutcome   = escTempContract_.update(
        telemetry.isEscTempValid(), now, SIGNAL_LOSS_GRACE_MS);
    SignalArmContract::Outcome battOutcome  = batteryContract_.update(
        telemetry.isBatteryVoltageValid(), now, SIGNAL_LOSS_GRACE_MS);

    if (motorOutcome == SignalArmContract::Outcome::LostInvalid) {
        throttle.setDisarmed(DisarmReason::MotorTempLost);
    } else if (motorOutcome == SignalArmContract::Outcome::SourceChanged) {
        throttle.setDisarmed(DisarmReason::MotorTempSourceChanged);
    }
    if (escOutcome == SignalArmContract::Outcome::LostInvalid) {
        throttle.setDisarmed(DisarmReason::EscTempLost);
    }
    if (battOutcome == SignalArmContract::Outcome::LostInvalid) {
        throttle.setDisarmed(DisarmReason::BatteryVoltageLost);
    }
}

unsigned int Power::getPwm() {
    // Every path below assigns before use; the initializer only silences
    // -Wmaybe-uninitialized, which can't prove the switch over StartState
    // (no default: case) is exhaustive.
    unsigned int resultPwm = ESC_MIN_PWM;
    if (!throttle.isCalibrated()) {
        resetMotorState();
        resultPwm = ESC_MIN_PWM;
    } else {
        unsigned int powerLimit  = getPower();
        unsigned int throttleMin = throttle.getThrottlePinMin();
        unsigned int throttleMax = throttle.getThrottlePinMax();
        unsigned int throttleRaw = throttle.isEngaged() ? throttle.getThrottleRaw() : throttleMin;

        unsigned int allowedMax = throttleMin + ((throttleMax - throttleMin) * powerLimit) / 100;
        unsigned int clampedRaw = constrain(throttleRaw, throttleMin, allowedMax);

        unsigned int range = throttleMax - throttleMin;
        float targetPwm;
        if (range == 0) {
            targetPwm = (float)ESC_MIN_PWM;
        } else {
            float norm = (float)(clampedRaw - throttleMin) / (float)range;
            targetPwm = (float)ESC_MIN_PWM + (float)(ESC_MAX_PWM - ESC_MIN_PWM) * norm;
        }
        targetPwm = constrain(targetPwm, (float)ESC_MIN_PWM, (float)ESC_MAX_PWM);

        bool throttleActive = (targetPwm > (float)(ESC_MIN_PWM + THROTTLE_DEADBAND_US));

        if (getBoardConfig().useSmoothStart) {
            unsigned long now = millis();

            switch (startState) {

            case StartState::IDLE:
                if (throttleActive) {
                    startState = StartState::STARTING;
                    startingBeganAt = now;
                }
                resultPwm = ESC_MIN_PWM;
                break;

            case StartState::STARTING: {
                if (!throttleActive) {
                    startState = StartState::IDLE;
                    resultPwm = ESC_MIN_PWM;
                    break;
                }
                float wakeupPwm = (float)ESC_MIN_PWM + (float)(ESC_MAX_PWM - ESC_MIN_PWM) * (float)XAG_WAKEUP_PWM_PERCENT / 100.0f;
                if (now - startingBeganAt >= XAG_MOTOR_REACTION_DELAY_MS) {
                    startState = StartState::RUNNING;
                    resultPwm = (unsigned int)targetPwm;
                } else {
                    resultPwm = (unsigned int)wakeupPwm;
                }
                break;
            }

            case StartState::RUNNING:
                if (!throttleActive) {
                    if (idleBeganAt == 0) idleBeganAt = now;

                    if (now - idleBeganAt >= MOTOR_STOP_TIME_MS) {
                        startState = StartState::IDLE;
                        idleBeganAt = 0;
                        resultPwm = ESC_MIN_PWM;
                    } else {
                        resultPwm = (unsigned int)targetPwm;
                    }
                } else {
                    idleBeganAt = 0;
                    resultPwm = (unsigned int)targetPwm;
                }
                break;
            }
        } else {
            resultPwm = (unsigned int)targetPwm;
        }
    }

    // Single exit: the disarm ramp applies over the span above ESC_MIN_PWM.
    // Scaling after the smooth-start machine means StartState stays RUNNING
    // during a ramp, so recovery does not re-enter the 1500 ms / 5% wake-up.
    // Harmless on the ESC_MIN_PWM early returns, but this is what makes that
    // obvious.
    return applyDisarmScale(resultPwm);
}

unsigned int Power::applyDisarmScale(unsigned int pwm) {
    if (disarmScale_ >= 100 || pwm <= ESC_MIN_PWM) {
        return pwm;
    }
    return ESC_MIN_PWM + (unsigned int)(((uint32_t)(pwm - ESC_MIN_PWM) * disarmScale_) / 100);
}

void Power::resetMotorState() {
    startState = StartState::IDLE;
    startingBeganAt = 0;
    idleBeganAt = 0;
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
    // limiting below — disarm-on-signal-loss is a separate safety behavior
    // handled by checkSignalLoss(), called independently every loop tick,
    // and is not affected by this setting.
    if (!settings.getPowerControlEnabled()) {
        activeLimitCauses_ = POWER_LIMIT_NONE;
        return 100;
    }
    unsigned int batteryLimit   = calcBatteryLimit();
    unsigned int motorTempLimit = calcMotorTempLimit();
    unsigned int escTempLimit   = calcEscTempLimit();

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
    if (!motorTempContract_.shouldLimit(telemetry.isMotorTempValid())) return 100;

    int32_t motorTempMilliCelsius = telemetry.getMotorTempMilliCelsius();
    int32_t reductionStart = settings.getMotorTempReductionStart();
    int32_t maxTemp        = settings.getMotorMaxTemp();

    if (motorTempMilliCelsius < reductionStart) return 100;
    if (reductionStart == maxTemp) return 0;

    return constrain(map(motorTempMilliCelsius, reductionStart, maxTemp, 100, 0), 0, 100);
}

unsigned int Power::calcEscTempLimit() {
    if (!escTempContract_.shouldLimit(telemetry.isEscTempValid())) return 100;

    int32_t escTempMilliCelsius = telemetry.getEscTempMilliCelsius();
    int32_t reductionStart = settings.getEscTempReductionStart();
    int32_t maxTemp        = settings.getEscMaxTemp();

    if (escTempMilliCelsius < reductionStart) return 100;
    if (reductionStart == maxTemp) return 0;

    return constrain(map(escTempMilliCelsius, reductionStart, maxTemp, 100, 0), 0, 100);
}

void Power::resetBatteryPowerFloor() {
    batteryPowerFloor = 100;
    resetMotorState();
}
