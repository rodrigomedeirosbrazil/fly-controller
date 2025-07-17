#include "Power.h"

Power::Power() {
    lastPowerCalculationTime = 0;
    pwm = ESC_MIN_PWM;
    power = 100;
    batteryPowerFloor = 100;
}

unsigned int Power::getPwm() {
    unsigned int throttleRaw = throttle.getThrottleRaw();
    unsigned int powerLimit = getPower(); // 0-100

    int throttleMin = throttle.getThrottlePinMin();
    int throttleMax = throttle.getThrottlePinMax();
    int allowedMax = throttleMin + ((throttleMax - throttleMin) * powerLimit) / 100;
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

    return min(batteryLimit, motorTempLimit);
}

unsigned int Power::calcBatteryLimit() {
    if (!canbus.isReady()) {
        return 0;
    }

    unsigned int batteryDeciVolts = canbus.getDeciVoltage();
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
}

unsigned int Power::calcMotorTempLimit() {
   double readedMotorTemp = motorTemp.getTemperature();

   if (readedMotorTemp < MOTOR_MAX_TEMP - 10) {
        return 100;
   }

   return constrain(
        map(
            readedMotorTemp,
            MOTOR_MAX_TEMP - 10,
            MOTOR_MAX_TEMP,
            100,
            0
        ),
        0,
        100
    );
}

void Power::resetBatteryPowerFloor() {
    batteryPowerFloor = 100;
}
