#ifndef POWER_H
#define POWER_H

#include <Arduino.h>

class Throttle;
class Canbus;
#ifndef XAG
#ifdef T_MOTOR
class Tmotor;
#else
class Hobbywing;
#endif
#endif
class Temperature;

class Power {
public:
    Power();
    unsigned int getPwm();
    unsigned int getPower();
    void resetBatteryPowerFloor();
#ifdef XAG
    unsigned int getBatteryVoltageDeciVolts();
#endif

private:
    long lastPowerCalculationTime;
    unsigned int pwm;
    unsigned int power;
    unsigned int batteryPowerFloor;

    unsigned int calcPower();
    unsigned int calcBatteryLimit();
    unsigned int calcMotorTempLimit();
    unsigned int calcEscTempLimit();
#ifdef XAG
    unsigned int readBatteryVoltageDeciVolts();
#endif
};

#endif // POWER_H
