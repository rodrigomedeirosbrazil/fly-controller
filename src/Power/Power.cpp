#include "Power.h"

int Power::getPwm() {
    if (millis() - lastPowerCalculationTime > 1000) {
        lastPowerCalculationTime = millis();
    }

    return ESC_MAX_PWM;
}
