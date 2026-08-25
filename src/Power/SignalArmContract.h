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
// 2. Loss is DEBOUNCED via graceMs. The signal must read "not effectively
//    valid" continuously for graceMs before update() reports a disarm, where
//    effectively valid means valid AND still coming from the source that was
//    armed (see the SOURCE TAG note below). A single effectively-valid sample
//    — valid at the armed source — resets the timer. Every other validity
//    path in this firmware has tolerance (wired throttle: 3 consecutive I2C
//    failures; wireless: 500 ms to ramp, 3 s to disarm), and the underlying
//    signals here are genuinely intermittent — a CAN ESC_STATUS transfer is
//    dropped whole if any one of its frames is lost. Temperature and pack
//    voltage cannot change dangerously within the grace window, so this costs
//    no real protection.
//
// The SOURCE TAG extends the contract with the same idea for a signal that
// has multiple sensors behind one reading (motor temp: CAN vs NTC). The
// snapshot captures which sensor produced the reading at arm (tagAtArm_),
// and "effective valid" is `valid && (tag == tagAtArm_)` — a change of
// sensor mid-flight is treated exactly like the armed sensor going invalid.
// update() reports which one happened so the caller can pick a specific
// disarm reason: `LostInvalid` when the reading itself went bad (also the
// precedence when both apply — an invalid reading has no source worth
// reporting), `SourceChanged` only when the new source reads valid but
// differs from the one the pilot armed with. Callers whose signal has a
// single source (ESC temp, battery voltage) pass `0`; their tag never
// changes, so the tag guard is inert and they behave exactly as before.
//
// The tag is a REQUIRED argument — there is no default. A forgotten tag
// would silently compare against 0 and, on Tmotor where the origin is never
// 0, permanently disable the limiter; making it mandatory turns that bug
// into a compile error.
//
// Threading: shouldLimit() is a pure read, safe to call from any task
// (calc*Limit() is reachable from the async web-server task). update() and
// onArmed() mutate state and must only be called from the main loop —
// see Power::checkSignalLoss().
class SignalArmContract {
public:
    enum class Outcome : uint8_t { None, LostInvalid, SourceChanged };

    SignalArmContract() { reset(); }

    void reset() {
        validAtArm_ = false;
        tagAtArm_ = 0;
        snapshotPending_ = false;
        invalidRunning_ = false;
        invalidSinceMs_ = 0;
    }

    // Call once, at the moment arming succeeds. Takes no validity argument
    // on purpose — see note 1 above.
    void onArmed() {
        validAtArm_ = false;
        tagAtArm_ = 0;
        snapshotPending_ = true;
        invalidRunning_ = false;
        invalidSinceMs_ = 0;
    }

    // Call once per main-loop tick while armed. Returns what (if anything)
    // should fire right now: `None` normally, `LostInvalid` when the signal
    // was valid at arm and has not been effectively valid continuously for
    // at least graceMs with an invalid reading on the expiry tick,
    // `SourceChanged` when the divergence was a pure source-tag change (the
    // reading stayed valid, only the tag differs).
    Outcome update(bool validNow, uint32_t nowMs, uint32_t graceMs, uint8_t tag) {
        if (snapshotPending_) {
            // First tick after arming: this sample IS the contract. Never
            // disarm on it — there is no "loss" to observe yet. The tag is
            // snapshotted on the same tick as validity, so the two can
            // never desync.
            validAtArm_ = validNow;
            tagAtArm_ = tag;
            snapshotPending_ = false;
            invalidRunning_ = false;
            return Outcome::None;
        }

        if (!validAtArm_) {
            // Invalid at arm: disabled for the whole session, never disarms —
            // even if a source appears later.
            return Outcome::None;
        }

        const bool effectiveValid = validNow && (tag == tagAtArm_);

        if (effectiveValid) {
            invalidRunning_ = false;
            return Outcome::None;
        }

        if (!invalidRunning_) {
            invalidRunning_ = true;
            invalidSinceMs_ = nowMs;
        }
        if ((uint32_t)(nowMs - invalidSinceMs_) < graceMs) {
            return Outcome::None;
        }

        // Precedence when both apply: an invalid reading has no source worth
        // reporting, so "sensor failed" wins over "source changed".
        return validNow ? Outcome::SourceChanged : Outcome::LostInvalid;
    }

    // True if this signal should currently be used for power limiting.
    // Pure read — safe from any task. The tag guard is NOT debounced: the
    // instant the source diverges, this stops driving the limiter, so the
    // limiter never runs one sensor's thresholds against another sensor's
    // reading — not even during the grace window (goal: no single loop
    // iteration on a mismatched sensor).
    bool shouldLimit(bool validNow, uint8_t tag) const {
        return validAtArm_ && validNow && (tag == tagAtArm_);
    }

private:
    bool     validAtArm_;
    uint8_t  tagAtArm_;
    bool     snapshotPending_;
    bool     invalidRunning_;
    uint32_t invalidSinceMs_;
};
