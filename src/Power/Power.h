#ifndef POWER_H
#define POWER_H

#include "../config.h"

class Throttle;
class Canbus;
class Temperature;

class Power {
public:
    unsigned int getPwm();
    unsigned int getPower();

private:
    long lastPowerCalculationTime;
    unsigned int pwm;
    unsigned int power;

    unsigned int calcPower();
};

#endif // POWER_H