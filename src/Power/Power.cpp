#include "Power.h"

Power::Power() {
    lastPowerCalculationTime = 0;
    pwm = ESC_MIN_PWM;
    power = 100;
}

unsigned int Power::getPwm() {
    unsigned int effectivePercent = (throttle.getThrottlePercentage() * getPower()) / 100;

    return map(
        effectivePercent,
        0,
        100,
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

    int batteryMilliVolts = canbus.getMiliVoltage();
    int batteryPercentage = map(
        batteryMilliVolts,
        BATTERY_MIN_VOLTAGE,
        BATTERY_MAX_VOLTAGE,
        0,
        100
    );

    if (batteryPercentage > 10) {
        return 100;
    }

    return map(
        batteryPercentage,
        0,
        10,
        100,
        0
    );
}

unsigned int Power::calcMotorTempLimit() {
   double readedMotorTemp = motorTemp.getTemperature();

   if (readedMotorTemp < MOTOR_MAX_TEMP - 10) {
        return 100;
   }

   return map(
        readedMotorTemp,
        MOTOR_MAX_TEMP - 10,
        MOTOR_MAX_TEMP,
        100,
        0
    );
}
