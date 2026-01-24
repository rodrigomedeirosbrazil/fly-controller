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
    unsigned int getBatteryVoltageDeciVolts(); // For backward compatibility (returns decivolts)

private:
    long lastPowerCalculationTime;
    unsigned int pwm;
    unsigned int power;
    unsigned int batteryPowerFloor;

    unsigned int calcPower();
    unsigned int calcBatteryLimit();
    unsigned int calcMotorTempLimit();
    unsigned int calcEscTempLimit();
};

#endif // POWER_H
