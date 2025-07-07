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
    return 100;
}


