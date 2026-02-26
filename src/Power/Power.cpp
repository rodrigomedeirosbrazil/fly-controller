#include "Power.h"
#include "../config.h"
#include "../BoardConfig.h"
#include "../Throttle/Throttle.h"
#include "../Temperature/Temperature.h"

extern Throttle throttle;
extern Temperature motorTemp;
extern Settings settings;

Power::Power() {
    lastPowerCalculationTime = 0;
    power = 100;
    batteryPowerFloor = 100;
    outputPwm = ESC_MIN_PWM;
    lastTickMs = 0;
}

unsigned int Power::getPwm() {
    if (!throttle.isCalibrated()) {
        resetRampLimiting();
        return (unsigned int)outputPwm;
    }

    unsigned long now = millis();

    if (lastTickMs == 0) {
        lastTickMs = now;
    }

    unsigned long dt = now - lastTickMs;
    if (dt > 50) dt = 50;
    lastTickMs = now;

    unsigned int powerLimit  = getPower();
    unsigned int throttleMin = throttle.getThrottlePinMin();
    unsigned int throttleMax = throttle.getThrottlePinMax();
    unsigned int throttleRaw = throttle.getThrottleRaw();

    unsigned int allowedMax  = throttleMin + ((throttleMax - throttleMin) * powerLimit) / 100;
    unsigned int clampedRaw  = constrain(throttleRaw, throttleMin, allowedMax);

    float targetPwm = (float)map(
        (long)clampedRaw,
        (long)throttleMin, (long)throttleMax,
        (long)ESC_MIN_PWM, (long)ESC_MAX_PWM
    );
    targetPwm = constrain(targetPwm, (float)ESC_MIN_PWM, (float)ESC_MAX_PWM);

    float delta = targetPwm - outputPwm;

    float maxUp   = THROTTLE_RAMP_UP_US_PER_MS   * (float)dt;
    float maxDown = THROTTLE_RAMP_DOWN_US_PER_MS  * (float)dt;

    if (delta > 0.0f) {
        outputPwm += min(delta, maxUp);
    } else if (delta < 0.0f) {
        outputPwm += max(delta, -maxDown);
    }

    outputPwm = constrain(outputPwm, (float)ESC_MIN_PWM, (float)ESC_MAX_PWM);

    return (unsigned int)outputPwm;
}

void Power::resetRampLimiting() {
    outputPwm  = (float)ESC_MIN_PWM;
    lastTickMs = 0;
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
    if (!settings.getPowerControlEnabled()) {
        return 100;
    }
    unsigned int batteryLimit   = calcBatteryLimit();
    unsigned int motorTempLimit = calcMotorTempLimit();
    unsigned int escTempLimit   = calcEscTempLimit();
    return min(min(batteryLimit, motorTempLimit), escTempLimit);
}

unsigned int Power::calcBatteryLimit() {
    if (!getBoardConfig().useBatteryLimit) {
        return 100;
    }
    if (!telemetry.hasData()) {
        return 0;
    }

    uint16_t batteryMilliVolts = telemetry.getBatteryVoltageMilliVolts();
    const unsigned int STEP_DECREASE = 5;

    if (batteryMilliVolts > settings.getBatteryMinVoltage()) {
        return batteryPowerFloor;
    }

    if (batteryPowerFloor < STEP_DECREASE) {
        batteryPowerFloor = 0;
        return batteryPowerFloor;
    }

    batteryPowerFloor = batteryPowerFloor - STEP_DECREASE;
    return batteryPowerFloor;
}

unsigned int Power::calcMotorTempLimit() {
    double readedMotorTemp = motorTemp.getTemperature();
    int32_t motorTempMilliCelsius = (int32_t)(readedMotorTemp * 1000.0);

    if (motorTempMilliCelsius < MOTOR_TEMP_MIN_VALID || motorTempMilliCelsius > MOTOR_TEMP_MAX_VALID) {
        return 100;
    }

    int32_t motorTempReductionStart = settings.getMotorTempReductionStart();
    int32_t motorMaxTemp            = settings.getMotorMaxTemp();

    if (motorTempMilliCelsius < motorTempReductionStart) {
        return 100;
    }

    if (motorTempReductionStart == motorMaxTemp) {
        return 0;
    }

    return constrain(
        map(motorTempMilliCelsius, motorTempReductionStart, motorMaxTemp, 100, 0),
        0, 100
    );
}

unsigned int Power::calcEscTempLimit() {
    if (!telemetry.hasData()) {
        return 100;
    }
    int32_t escTempMilliCelsius = telemetry.getEscTempMilliCelsius();

    if (escTempMilliCelsius < ESC_TEMP_MIN_VALID || escTempMilliCelsius > ESC_TEMP_MAX_VALID) {
        return 100;
    }

    int32_t escTempReductionStart = settings.getEscTempReductionStart();
    int32_t escMaxTemp            = settings.getEscMaxTemp();

    if (escTempMilliCelsius < escTempReductionStart) {
        return 100;
    }

    if (escTempReductionStart == escMaxTemp) {
        return 0;
    }

    return constrain(
        map(escTempMilliCelsius, escTempReductionStart, escMaxTemp, 100, 0),
        0, 100
    );
}

void Power::resetBatteryPowerFloor() {
    batteryPowerFloor = 100;
    resetRampLimiting();
}
