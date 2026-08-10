#pragma once

#include <Arduino.h>
#include <Preferences.h>

// Flight-time session counter redesign:
// docs/superpowers/specs/2026-08-10-flight-time-counter.md
//
// The session timer pauses (instead of resetting) on disarm and while the
// motor is stopped, accumulates across arm/disarm cycles within a power
// cycle, and gains a manual reset via POST /api/session/reset. Implementation
// is deferred to a later commit on this branch.

class HourMeter {
public:
    void init();
    void handle(bool isArmed, bool motorRunning);
    uint32_t getHourMeterSec() const;
    uint32_t getSessionSec() const;

private:
    Preferences prefs_;
    uint32_t hourMeterSec_ = 0;
    uint32_t sessionSec_ = 0;
    unsigned long motorRunStartMs_ = 0;
    unsigned long sessionStartMs_ = 0;
    unsigned long lastNvsSaveMs_ = 0;
    bool wasArmed_ = false;
    bool wasMotorRunning_ = false;

    void saveToNvs();
};
