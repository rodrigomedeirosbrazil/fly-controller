#ifndef POWER_H
#define POWER_H

#include <Arduino.h>
#include "SignalArmContract.h"

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
    void resetMotorState();

    // Snapshots each power-limiting signal's current validity for the
    // arm-time contract. Called by Throttle::setArmed() on a successful
    // arm — see SignalArmContract.h.
    void onArmed();

    // Evaluates each power-limiting signal's arm-time contract and disarms
    // if a signal that was valid at arm has since gone invalid. Must be
    // called ONLY from the main loop task (main.cpp's loop()) — never from
    // an async context. getPower()/calcPower() are reachable from the
    // AsyncWebServer/AsyncTCP task (via /api/telemetry) as well as the main
    // loop (via handleEsc(), Xctod, TelemetryLogger), so the disarm side
    // effect (which mutates Throttle/Buzzer state with no synchronization)
    // cannot safely live inside calc*Limit() — this method is the only
    // place that triggers it.
    //
    // calc*Limit()/getPower() themselves are NOT fully pure with respect to
    // this class's own state — calcBatteryLimit() still decrements
    // batteryPowerFloor, and getPower()'s 500ms cache is a plain
    // check-then-write on lastPowerCalculationTime/power/activeLimitCauses_.
    // That race is pre-existing (it predates this file's signal-validity
    // work and is reachable from the same AsyncTCP path today) and is out
    // of scope here; only the disarm decision was moved out to close the
    // hazard this change is responsible for.
    void checkSignalLoss();

private:
    enum class StartState {
        IDLE,
        STARTING,
        RUNNING,
    };

    long lastPowerCalculationTime;
    unsigned int power;
    unsigned int batteryPowerFloor;

    StartState startState;
    unsigned long startingBeganAt;
    unsigned long idleBeganAt;

    uint8_t activeLimitCauses_;

    SignalArmContract motorTempContract_;
    SignalArmContract escTempContract_;
    SignalArmContract batteryContract_;

    unsigned int calcPower();
    unsigned int calcBatteryLimit();
    unsigned int calcMotorTempLimit();
    unsigned int calcEscTempLimit();
};

#endif // POWER_H
