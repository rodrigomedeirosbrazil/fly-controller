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
    unsigned int pwm;
    unsigned int power;
    unsigned int batteryPowerFloor;
    int prevPwm;

    // Smooth start state (used when BoardConfig::useSmoothStart)
    bool smoothStartActive;
    unsigned long motorStoppedTime;
    unsigned long smoothStartBeginTime;
    int smoothStartInitialPwm;
    bool preStartActive;
    unsigned long preStartBeginTime;
    int preStartPwm;

    unsigned int calcPower();
    unsigned int calcBatteryLimit();
    unsigned int calcMotorTempLimit();
    unsigned int calcEscTempLimit();
    unsigned int applyRampLimiting(int targetPwm);
    bool detectMotorStopped();
    unsigned int applyPreStart();
    unsigned int applySmoothStart(int targetPwm);
};

#endif // POWER_H
