#include "Power.h"

Power::Power(int escMinPwm, int escMaxPwm)
    : escMinPwm(escMinPwm), escMaxPwm(escMaxPwm), pwm(escMinPwm), lastThrottlePercent(0), lastBatteryPercent(100), lastEscTemp(0), lastMotorTemp(0), lastCurrent(0) {}

void Power::update(float throttlePercent, float batteryPercent, float escTemp, float motorTemp, float current) {
    lastThrottlePercent = throttlePercent;
    lastBatteryPercent = batteryPercent;
    lastEscTemp = escTemp;
    lastMotorTemp = motorTemp;
    lastCurrent = current;

    float batteryLimit = calcBatteryLimit(batteryPercent);
    float escTempLimit = calcEscTempLimit(escTemp);
    float motorTempLimit = calcMotorTempLimit(motorTemp);
    float currentLimit = calcCurrentLimit(current);

    float limiter = batteryLimit;
    if (escTempLimit < limiter) limiter = escTempLimit;
    if (motorTempLimit < limiter) limiter = motorTempLimit;
    if (currentLimit < limiter) limiter = currentLimit;

    float limitedThrottle = throttlePercent * limiter;
    pwm = escMinPwm + (int)((escMaxPwm - escMinPwm) * (limitedThrottle / 100.0f));
}

int Power::getPwm() const {
    return pwm;
}

float Power::calcBatteryLimit(float batteryPercent) const {
    if (batteryPercent > 10.0f) return 1.0f;
    if (batteryPercent <= 0.0f) return 0.0f;
    return batteryPercent / 10.0f;
}

float Power::calcEscTempLimit(float escTemp) const {
    // TODO: Implementar lógica real
    return 1.0f;
}

float Power::calcMotorTempLimit(float motorTemp) const {
    // TODO: Implementar lógica real
    return 1.0f;
}

float Power::calcCurrentLimit(float current) const {
    // TODO: Implementar lógica real
    return 1.0f;
}