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
        throttle->getThrottlePercentage(),
        0,
        100,
        ESC_MIN_PWM,
        ESC_MAX_PWM
    );

    esc.writeMicroseconds(pulseWidth);
}

void Esc::setPowerAvailable(unsigned int power) {
    if (millis() - lastPowerAvailableChange < minTimeToChangePowerAvailable) {
        return;
    }

    powerAvailable = power > 100
        ? 100
        : (power < 0 ? 0 : power);

    lastPowerAvailableChange = millis();
}
