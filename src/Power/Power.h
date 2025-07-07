#ifndef POWER_H
#define POWER_H

#include "../config.h"

class Throttle;
class Canbus;
class Temperature;

class Power {
public:
    int getPwm();

private:
    long lastPowerCalculationTime;
};

#endif // POWER_H