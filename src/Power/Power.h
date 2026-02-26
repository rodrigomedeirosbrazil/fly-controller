#ifndef POWER_H
#define POWER_H

#include <Arduino.h>

class Throttle;
class Temperature;

class Power {
public:
    Power();
    unsigned int getPwm();
    unsigned int getPower();
    void resetBatteryPowerFloor();
    void resetRampLimiting();

private:
    long lastPowerCalculationTime;
    unsigned int power;
    unsigned int batteryPowerFloor;

    float outputPwm;
    unsigned long lastTickMs;

    unsigned int calcPower();
    unsigned int calcBatteryLimit();
    unsigned int calcMotorTempLimit();
    unsigned int calcEscTempLimit();
};

#endif // POWER_H
