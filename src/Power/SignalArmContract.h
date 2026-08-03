// src/Power/SignalArmContract.h
#pragma once

#include <stdint.h>

// Pure "arm-time contract" decision for a single power-limiting signal —
// no Arduino deps, host-testable.
//
// The pilot arms knowing what protection is currently active. A signal
// invalid at arming time stays disabled for the whole session, even if it
// recovers — a recovered-then-refaulting sensor would otherwise cut the
// motor in flight on a protection the pilot knowingly accepted going
// without. A signal valid at arming time that later goes invalid is a
// promise broken mid-flight — that disarms.
//
// Two properties exist to keep that disarm from firing on noise:
//
// 1. The arm snapshot is DEFERRED. onArmed() only marks a snapshot as due;
//    the value is captured by the next update(). Arming happens in
//    button.check(), at the top of loop(), while the telemetry cache the
//    validity comes from is refreshed near the bottom — so a snapshot taken
//    inside onArmed() would read the previous iteration's sample and then be
//    compared against this iteration's, with every slow component in between
//    (BLE, web server) widening the gap. On a signal whose validity is a
//    freshness window (CAN ESC telemetry: 1 s), that gap alone was enough to
//    read "valid at arm, invalid now" on the very first check and disarm
//    immediately on arming. Snapshotting and comparing against the same
//    sample makes that straddle impossible.
//
// 2. Loss is DEBOUNCED via graceMs. The signal must read invalid
//    continuously for graceMs before update() reports a disarm; a single
//    valid sample resets the timer. Every other validity path in this
//    firmware has tolerance (wired throttle: 3 consecutive I2C failures;
//    wireless: 500 ms to ramp, 3 s to disarm), and the underlying signals
//    here are genuinely intermittent — a CAN ESC_STATUS transfer is dropped
//    whole if any one of its frames is lost. Temperature and pack voltage
//    cannot change dangerously within the grace window, so this costs no
//    real protection.
//
// Threading: shouldLimit() is a pure read, safe to call from any task
// (calc*Limit() is reachable from the async web-server task). update() and
// onArmed() mutate state and must only be called from the main loop —
// see Power::checkSignalLoss().
class SignalArmContract {
public:
    SignalArmContract() { reset(); }

    void reset() {
        validAtArm_ = false;
        snapshotPending_ = false;
        invalidRunning_ = false;
        invalidSinceMs_ = 0;
    }

    // Call once, at the moment arming succeeds. Takes no validity argument
    // on purpose — see note 1 above.
    void onArmed() {
        validAtArm_ = false;
        snapshotPending_ = true;
        invalidRunning_ = false;
        invalidSinceMs_ = 0;
    }

    // Call once per main-loop tick while armed. Returns true when a disarm
    // should fire now (signal was valid at arm and has read invalid
    // continuously for at least graceMs).
    bool update(bool validNow, uint32_t nowMs, uint32_t graceMs) {
        if (snapshotPending_) {
            // First tick after arming: this sample IS the contract. Never
            // disarm on it — there is no "loss" to observe yet.
            validAtArm_ = validNow;
            snapshotPending_ = false;
            invalidRunning_ = false;
            return false;
        }

        if (!validAtArm_) {
            // Invalid at arm: disabled for the whole session, never disarms.
            return false;
        }

        if (validNow) {
            invalidRunning_ = false;
            return false;
        }

        if (!invalidRunning_) {
            invalidRunning_ = true;
            invalidSinceMs_ = nowMs;
        }
        return (uint32_t)(nowMs - invalidSinceMs_) >= graceMs;
    }

    // True if this signal should currently be used for power limiting.
    // Pure read — safe from any task.
    bool shouldLimit(bool validNow) const { return validAtArm_ && validNow; }

private:
    bool     validAtArm_;
    bool     snapshotPending_;
    bool     invalidRunning_;
    uint32_t invalidSinceMs_;
};
