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
    startState = StartState::IDLE;
    startingBeganAt = 0;
    idleBeganAt = 0;
}

void Power::onArmed() {
    motorTempContract_.onArmed(telemetry.isMotorTempValid());
    escTempContract_.onArmed(telemetry.isEscTempValid());
    batteryContract_.onArmed(telemetry.isBatteryVoltageValid());
}

void Power::checkSignalLoss() {
    if (motorTempContract_.shouldDisarmOnLoss(telemetry.isMotorTempValid())) {
        throttle.setDisarmed(DisarmReason::MotorTempLost);
    }
    if (escTempContract_.shouldDisarmOnLoss(telemetry.isEscTempValid())) {
        throttle.setDisarmed(DisarmReason::EscTempLost);
    }
    if (batteryContract_.shouldDisarmOnLoss(telemetry.isBatteryVoltageValid())) {
        throttle.setDisarmed(DisarmReason::BatteryVoltageLost);
    }
}

unsigned int Power::getPwm() {
    if (!throttle.isCalibrated()) {
        resetMotorState();
        return ESC_MIN_PWM;
    }

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
            return ESC_MIN_PWM;

        case StartState::STARTING: {
            if (!throttleActive) {
                startState = StartState::IDLE;
                return ESC_MIN_PWM;
            }
            float wakeupPwm = (float)ESC_MIN_PWM + (float)(ESC_MAX_PWM - ESC_MIN_PWM) * (float)XAG_WAKEUP_PWM_PERCENT / 100.0f;
            if (now - startingBeganAt >= XAG_MOTOR_REACTION_DELAY_MS) {
                startState = StartState::RUNNING;
                return (unsigned int)targetPwm;
            }
            return (unsigned int)wakeupPwm;
        }

        case StartState::RUNNING:
            if (!throttleActive) {
                if (idleBeganAt == 0) idleBeganAt = now;

                if (now - idleBeganAt >= MOTOR_STOP_TIME_MS) {
                    startState = StartState::IDLE;
                    idleBeganAt = 0;
                    return ESC_MIN_PWM;
                }
            } else {
                idleBeganAt = 0;
            }
            return (unsigned int)targetPwm;
        }
    }

    return (unsigned int)targetPwm;
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
