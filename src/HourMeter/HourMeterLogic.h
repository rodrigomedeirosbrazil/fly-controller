#pragma once

#include <stdint.h>
#include <stdbool.h>

// Pure state machine for the flight-time hour meter. Header-only and free of
// Arduino deps so it can be host-tested with `c++ -std=c++17` like the other
// Logic headers (PeriodicTrigger, PowerAlertLogic, ThrottleSignalLogic).
//
// Two counters:
//   - sessionSec:    flight time for the current power cycle (RAM only).
//                    Advances only while (isArmed && motorRunning), so it
//                    pauses on disarm and while the motor is stopped, and
//                    resumes from the same value on the next run. Reset via
//                    requestReset().
//   - hourMeterSec:  persistent total motor-run seconds, loaded/saved by the
//                    Arduino wrapper. Same semantics as the original HourMeter:
//                    accumulates while the motor runs and is flushed to NVS on
//                    motor stop / disarm / periodic save.
//
// Counting rule (running == isArmed && motorRunning):
//   - paused -> running: start an interval (sessionStartMs / motorRunStartMs = now)
//   - running -> paused: fold the interval into the committed totals
//   - unchanged:         nothing
//
// requestReset() only sets a flag; the reset is applied on the next tick() so a
// caller on another task (web server) never touches loop-owned state — the same
// deferral pattern as Throttle::setArmed()/setDisarmed().
struct HourMeterLogic {
    uint32_t sessionSec = 0;        // committed session seconds (folded intervals)
    uint32_t hourMeterSec = 0;      // committed motor-run seconds (persistent)
    uint32_t sessionStartMs = 0;    // running interval start; 0 == paused
    uint32_t motorRunStartMs = 0;   // running motor interval start; 0 == stopped
    bool wasRunning = false;        // previous running predicate
    bool resetRequested = false;

    // Request the session counter be cleared. Applied on the next tick().
    void requestReset() { resetRequested = true; }

    void tick(bool isArmed, bool motorRunning, uint32_t nowMs) {
        // Consume a pending reset. If currently running, re-seed the session
        // interval at "now" so the in-flight seconds are discarded, not
        // re-added on the very next tick.
        if (resetRequested) {
            sessionSec = 0;
            sessionStartMs = wasRunning ? nowMs : 0;
            resetRequested = false;
        }

        bool nowRunning = isArmed && motorRunning;
        if (nowRunning && !wasRunning) {
            sessionStartMs = nowMs;
            motorRunStartMs = nowMs;
        } else if (!nowRunning && wasRunning) {
            sessionSec += (nowMs - sessionStartMs) / 1000;
            sessionStartMs = 0;
            hourMeterSec += (nowMs - motorRunStartMs) / 1000;
            motorRunStartMs = 0;
        }
        wasRunning = nowRunning;
    }

    uint32_t getSessionSec(uint32_t nowMs) const {
        if (wasRunning) {
            return sessionSec + (nowMs - sessionStartMs) / 1000;
        }
        return sessionSec;
    }

    uint32_t getHourMeterSec(uint32_t nowMs) const {
        if (wasRunning) {
            return hourMeterSec + (nowMs - motorRunStartMs) / 1000;
        }
        return hourMeterSec;
    }
};
