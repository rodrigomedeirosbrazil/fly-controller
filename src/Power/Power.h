#ifndef POWER_H
#define POWER_H

#include <Arduino.h>

class Throttle;
class Canbus;
class Hobbywing;
class Temperature;

class Power {
public:
    Power();
    unsigned int getPwm();
    unsigned int getPower();
    void resetBatteryPowerFloor();

private:
    long lastPowerCalculationTime;
    unsigned int pwm;
    unsigned int power;
    unsigned int batteryPowerFloor;

    unsigned int calcPower();
    unsigned int calcBatteryLimit();
    unsigned int calcMotorTempLimit();
};

#endif // POWER_H
