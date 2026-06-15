#ifndef POWER_H
#define POWER_H

#include <Arduino.h>

enum PowerLimitCause : uint8_t {
    POWER_LIMIT_NONE       = 0,
    POWER_LIMIT_BATTERY    = 1 << 0,
    POWER_LIMIT_MOTOR_TEMP = 1 << 1,
    POWER_LIMIT_ESC_TEMP   = 1 << 2,
};

class Throttle;
class Temperature;

class Power {
public:
    Power();
    unsigned int getPwm();
    unsigned int getPower();
    uint8_t getActiveLimitCauses() const { return activeLimitCauses_; }
    void resetBatteryPowerFloor();
    void resetRampLimiting();

private:
    enum class StartState {
        IDLE,
        STARTING,
        RUNNING,
    };

    long lastPowerCalculationTime;
    unsigned int power;
    unsigned int batteryPowerFloor;

    float outputPwm;
    unsigned long lastTickMs;

    StartState startState;
    unsigned long startingBeganAt;
    unsigned long idleBeganAt;
    float startingPwmCap;

    uint8_t activeLimitCauses_;

    unsigned int calcPower();
    unsigned int calcBatteryLimit();
    unsigned int calcMotorTempLimit();
    unsigned int calcEscTempLimit();
};

#endif // POWER_H
