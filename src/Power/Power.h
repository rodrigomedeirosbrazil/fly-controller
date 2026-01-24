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
    void resetRampLimiting(); // Reset ramp limiting state (prevPwm)
    unsigned int getBatteryVoltageDeciVolts(); // For backward compatibility (returns decivolts)

private:
    long lastPowerCalculationTime;
    unsigned int pwm;
    unsigned int power;
    unsigned int batteryPowerFloor;
    int prevPwm; // Previous PWM value for ramp limiting

    unsigned int calcPower();
    unsigned int calcBatteryLimit();
    unsigned int calcMotorTempLimit();
    unsigned int calcEscTempLimit();
    unsigned int applyRampLimiting(int targetPwm); // Apply ramp limiting to smooth acceleration/deceleration
};

#endif // POWER_H
