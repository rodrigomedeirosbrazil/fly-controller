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

    #if IS_XAG
    // Smooth start state (XAG only)
    bool smoothStartActive; // Flag indicating if smooth start is active
    unsigned long motorStoppedTime; // Timestamp when motor was detected as stopped
    unsigned long smoothStartBeginTime; // Timestamp when smooth start began
    int smoothStartInitialPwm; // Initial PWM value for smooth start (ESC_MIN_PWM)
    #endif

    unsigned int calcPower();
    unsigned int calcBatteryLimit();
    unsigned int calcMotorTempLimit();
    unsigned int calcEscTempLimit();
    unsigned int applyRampLimiting(int targetPwm); // Apply ramp limiting to smooth acceleration/deceleration

    #if IS_XAG
    bool detectMotorStopped(); // Detect if motor is stopped (PWM at minimum for required time)
    unsigned int applySmoothStart(int targetPwm); // Apply smooth start ramp (linear interpolation over 2 seconds)
    #endif
};

#endif // POWER_H
