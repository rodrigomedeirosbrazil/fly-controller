// src/BatteryMonitor/BatteryRecalibrationLogic.h
// Pure decision of WHEN the coulomb count is recalibrated from the pack voltage.
// No Arduino or config includes; callers pass the time in, and time comparisons
// use unsigned subtraction so millis() rollover is safe.
//
// Rest is defined by "motor stopped", not by a current threshold: the Tmotor
// reports a residual current at idle, so any fixed threshold would have to be
// guessed per board and could keep the recalibration from ever firing.
//
// "Quiet" means the motor is not running and the voltage is valid, continuously.
// Any tick that is not quiet restarts the quiet clock.
//  - Boot fires once per power-on, the first time quiet lasts BATTERY_BOOT_SETTLE_MS.
//    It consumes the stop event it happened in: no Rest until the motor has run.
//  - Rest fires once per stop event when quiet lasts BATTERY_REST_SETTLE_MS, and
//    re-arms only after a tick with the motor running (an invalid-voltage gap
//    restarts the clock but does not re-arm).
#pragma once

#include <stdint.h>

enum class BatteryRecalibration : uint8_t { None = 0, Boot = 1, Rest = 2 };

struct BatteryRecalibrationInput {
    uint32_t nowMs;
    bool     motorRunning;   // isMotorRunning()
    bool     voltageValid;   // Telemetry::isBatteryVoltageValid()
};

static const uint32_t BATTERY_BOOT_SETTLE_MS = 2000;   // quiet time before the boot calibration
static const uint32_t BATTERY_REST_SETTLE_MS = 30000;  // quiet time before a rest recalibration

class BatteryRecalibrationLogic {
public:
    BatteryRecalibration tick(const BatteryRecalibrationInput& in) {
        if (in.motorRunning) {
            restDone_ = false;
        }
        if (in.motorRunning || !in.voltageValid) {
            quiet_ = false;
            return BatteryRecalibration::None;
        }
        if (!quiet_) {
            quiet_ = true;
            quietSinceMs_ = in.nowMs;
        }
        const uint32_t quietFor = in.nowMs - quietSinceMs_;
        if (bootPending_) {
            if (quietFor >= BATTERY_BOOT_SETTLE_MS) {
                bootPending_ = false;
                restDone_ = true;
                return BatteryRecalibration::Boot;
            }
            return BatteryRecalibration::None;
        }
        if (!restDone_ && quietFor >= BATTERY_REST_SETTLE_MS) {
            restDone_ = true;
            return BatteryRecalibration::Rest;
        }
        return BatteryRecalibration::None;
    }

    bool bootPending() const { return bootPending_; }

private:
    bool     bootPending_ = true;
    bool     restDone_ = false;
    bool     quiet_ = false;
    uint32_t quietSinceMs_ = 0;
};
