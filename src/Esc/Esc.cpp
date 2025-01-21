#include <Arduino.h>
#include "Esc.h"

#include "../config.h"

Esc::Esc(Canbus *canbus, Throttle *throttle)
{
    this->canbus = canbus;
    this->throttle = throttle;
}

void Esc::handle()
{
    if (canbus->isReady() && canbus->getMiliVoltage() <= BATTERY_MIN_VOLTAGE) {
        if (
          esc.attached()
          && !throttle->isSmoothThrottleChanging()
          && throttle->getThrottlePercentage() < 5
        ) {
            esc.detach();
            return;
        }

        if (
            esc.attached()
            && !throttle->isSmoothThrottleChanging()
        ) {
            canbus->setLedColor(Canbus::ledColorRed);
            throttle->setSmoothThrottleChange(
              throttle->getThrottlePercentage(),
              0
            );
        }

        if (! esc.attached()) {
            return;
        }
    }

    if (!throttle->isArmed()) {
        if (esc.attached()) {
            esc.detach();
            canbus->setLedColor(Canbus::ledColorRed);
        }
        return;
    }

    if (!esc.attached()) {
        esc.attach(ESC_PIN);
        canbus->setLedColor(Canbus::ledColorGreen);
    }

    int pulseWidth = ESC_MIN_PWM;

    pulseWidth = map(
        analizeTelemetryToThrottleOutput(
            throttle->getThrottlePercentage()),
        0,
        100,
        ESC_MIN_PWM,
        ESC_MAX_PWM);

    esc.writeMicroseconds(pulseWidth);
}

unsigned int Esc::analizeTelemetryToThrottleOutput(unsigned int throttlePercentage)
{
    #if ENABLED_LIMIT_THROTTLE
    if (
        canbus.isReady() && canbus.getTemperature() >= ESC_MAX_TEMP) {
        throttle->cancelCruise();

        return throttlePercentage < THROTTLE_RECOVERY_PERCENTAGE
                   ? throttlePercentage
                   : THROTTLE_RECOVERY_PERCENTAGE;
    }

    if (motorTemp.getTemperature() >= MOTOR_MAX_TEMP) {
        throttle->cancelCruise();

        return throttlePercentage < THROTTLE_RECOVERY_PERCENTAGE
                   ? throttlePercentage
                   : THROTTLE_RECOVERY_PERCENTAGE;
    }

    unsigned int miliCurrentLimit = ESC_MAX_CURRENT < BATTERY_MAX_CURRENT
                                        ? ESC_MAX_CURRENT * 10
                                        : BATTERY_MAX_CURRENT * 10;

    if (
        canbus.isReady() && canbus.getMiliCurrent() >= miliCurrentLimit) {
        throttle->cancelCruise();

        if (!isCurrentLimitReached) {
            isCurrentLimitReached = true;
            currentLimitReachedTime = millis();
            return throttlePercentage;
        }

        if (millis() - currentLimitReachedTime > 10000) {
            return throttlePercentage < THROTTLE_RECOVERY_PERCENTAGE
                       ? throttlePercentage
                       : THROTTLE_RECOVERY_PERCENTAGE;
        }

        if (millis() - currentLimitReachedTime > 3000) {
            return throttlePercentage - 10;
        }

        return throttlePercentage;
    }

    isCurrentLimitReached = false;
    currentLimitReachedTime = 0;

#endif

    return throttle->isSmoothThrottleChanging()
               ? throttle->getThrottlePercentageOnSmoothChange()
               : throttle->isCruising()
                    ? throttle->getCruisingThrottlePosition()
                    : throttlePercentage;
}