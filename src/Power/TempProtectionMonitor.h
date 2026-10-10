// src/Power/TempProtectionMonitor.h
#pragma once

#include <stdint.h>

// Pure debounced "is this temperature protection suspended?" state for one
// signal (motor temp or ESC temp) — no Arduino deps, host-testable.
//
// A temperature sensor that stops producing a trustworthy reading never cuts
// the motor: its derating simply stops applying, and the pilot is alarmed
// (see TempProtectionAlert). The limiter itself reacts on the same tick —
// Power::calc*Limit() checks the raw reading, never this state — so derating
// never runs on a reading that isn't there. This class only decides when the
// pilot is TOLD about it.
//
// Both edges are debounced by the same window: the signal must read the
// opposite way continuously for debounceMs before the state flips. A CAN
// ESC_STATUS transfer is dropped whole if any one of its frames is lost, so
// without the window a flaky bus would alternate the lost and restored
// sounds on every dropout. A single sample agreeing with the current state
// resets the pending transition.
//
// The monitor runs whether armed or not, so a sensor that is already out
// when the pilot arms is reported the moment arming succeeds.
class TempProtectionMonitor {
public:
    TempProtectionMonitor() : suspended_(false), pending_(false), pendingSinceMs_(0) {}

    // Call once per main-loop tick. `protectedNow` is true when the reading
    // the limiter trusts is valid right now (Power decides what that means
    // per signal).
    void update(bool protectedNow, uint32_t nowMs, uint32_t debounceMs) {
        const bool wantSuspended = !protectedNow;
        if (wantSuspended == suspended_) {
            pending_ = false;
            return;
        }
        if (!pending_) {
            pending_ = true;
            pendingSinceMs_ = nowMs;
        }
        if ((uint32_t)(nowMs - pendingSinceMs_) >= debounceMs) {
            suspended_ = wantSuspended;
            pending_ = false;
        }
    }

    bool isSuspended() const { return suspended_; }

private:
    bool     suspended_;
    bool     pending_;
    uint32_t pendingSinceMs_;
};
