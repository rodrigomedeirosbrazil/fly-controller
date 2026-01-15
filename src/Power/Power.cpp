#include "Power.h"
#include "../config.h"
#include "../Throttle/Throttle.h"
#ifndef XAG
#include "../Hobbywing/Hobbywing.h"
#endif
#include "../Temperature/Temperature.h"

extern Throttle throttle;
#ifndef XAG
extern Hobbywing hobbywing;
#endif
extern Temperature motorTemp;
#ifdef XAG
extern Temperature escTemp;
#endif

Power::Power() {
    lastPowerCalculationTime = 0;
    pwm = ESC_MIN_PWM;
    power = 100;
    batteryPowerFloor = 100;
}

unsigned int Power::getPwm() {
    unsigned int throttleRaw = throttle.getThrottleRaw();
    unsigned int powerLimit = getPower(); // 0-100

    unsigned int throttleMin = throttle.getThrottlePinMin();
    unsigned int throttleMax = throttle.getThrottlePinMax();

    // Check if throttle is calibrated (min and max are different)
    if (throttleMin == throttleMax) {
        return ESC_MIN_PWM;
    }

    unsigned int allowedMax = throttleMin + ((throttleMax - throttleMin) * powerLimit) / 100;
    unsigned int effectiveRaw = constrain(throttleRaw, throttleMin, allowedMax);

    return map(
        effectiveRaw,
        throttleMin,
        throttleMax,
        ESC_MIN_PWM,
        ESC_MAX_PWM
    );
}

unsigned int Power::getPower() {
    if (millis() - lastPowerCalculationTime < 1000) {
        return power;
    }

    lastPowerCalculationTime = millis();
    power = calcPower();

    return power;
}

unsigned int Power::calcPower() {
    unsigned int batteryLimit = calcBatteryLimit();
    unsigned int motorTempLimit = calcMotorTempLimit();
    unsigned int escTempLimit = calcEscTempLimit();

    return min(min(batteryLimit, motorTempLimit), escTempLimit);
}

unsigned int Power::calcBatteryLimit() {
#ifdef XAG
    // XAG mode: no battery voltage limit (always return 100)
    return 100;
#else
    if (!hobbywing.isReady()) {
        return 0;
    }

    unsigned int batteryDeciVolts = hobbywing.getDeciVoltage();
    const unsigned int STEP_DECREASE = 5;

    if (batteryDeciVolts > BATTERY_MIN_VOLTAGE) {
        return batteryPowerFloor;
    }

    if (batteryPowerFloor < STEP_DECREASE) {
        batteryPowerFloor = 0;
        return batteryPowerFloor;
    }

    batteryPowerFloor = batteryPowerFloor - STEP_DECREASE;

    return batteryPowerFloor;
#endif
}

unsigned int Power::calcMotorTempLimit() {
    double readedMotorTemp = motorTemp.getTemperature();

    // Validate temperature reading
    if (readedMotorTemp < MOTOR_TEMP_MIN_VALID || readedMotorTemp > MOTOR_TEMP_MAX_VALID) {
        return 100; // Invalid sensor data
    }

    if (readedMotorTemp < MOTOR_TEMP_REDUCTION_START) {
        return 100;
    }

    // Check if temperature range is valid (min != max)
    if (MOTOR_TEMP_REDUCTION_START == MOTOR_MAX_TEMP) {
        return 0; // If min == max, return 0 (maximum reduction)
    }

    return constrain(
        map(
            readedMotorTemp,
            MOTOR_TEMP_REDUCTION_START,
            MOTOR_MAX_TEMP,
            100,
            0
        ),
        0,
        100
    );
}

unsigned int Power::calcEscTempLimit() {
#ifdef XAG
    // XAG mode: use ESC temperature sensor (NTC)
    double escTempValue = escTemp.getTemperature();

    // Validate temperature reading
    if (escTempValue < ESC_TEMP_MIN_VALID || escTempValue > ESC_TEMP_MAX_VALID) {
        return 100; // Invalid sensor data
    }

    if (escTempValue < ESC_TEMP_REDUCTION_START) {
        return 100;
    }

    // Check if temperature range is valid (min != max)
    if (ESC_TEMP_REDUCTION_START == ESC_MAX_TEMP) {
        return 0; // If min == max, return 0 (maximum reduction)
    }

    return constrain(
        map(
            (int)escTempValue,
            ESC_TEMP_REDUCTION_START,
            ESC_MAX_TEMP,
            100,
            0
        ),
        0,
        100
    );
#else
    if (!hobbywing.isReady()) {
        return 100; // ESC not ready, allow other limits to control
    }

    uint8_t escTempValue = hobbywing.getTemperature();

    // Validate temperature reading
    if (escTempValue < ESC_TEMP_MIN_VALID || escTempValue > ESC_TEMP_MAX_VALID) {
        return 100; // Invalid sensor data
    }

    if (escTempValue < ESC_TEMP_REDUCTION_START) {
        return 100;
    }

    // Check if temperature range is valid (min != max)
    if (ESC_TEMP_REDUCTION_START == ESC_MAX_TEMP) {
        return 0; // If min == max, return 0 (maximum reduction)
    }

    return constrain(
        map(
            escTempValue,
            ESC_TEMP_REDUCTION_START,
            ESC_MAX_TEMP,
            100,
            0
        ),
        0,
        100
    );
#endif
}

void Power::resetBatteryPowerFloor() {
    batteryPowerFloor = 100;
}
