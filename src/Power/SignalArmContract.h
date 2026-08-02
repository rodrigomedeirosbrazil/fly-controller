// src/Power/SignalArmContract.h
#pragma once

// Pure "arm-time contract" decision for a single power-limiting signal —
// no Arduino deps, host-testable. See
// docs/superpowers/specs/2026-08-01-signal-validity-design.md, "Power-limiting
// sensors" section.
//
// The pilot arms knowing what protection is currently active. A signal
// invalid at arming time stays disabled for the whole session, even if it
// recovers — a recovered-then-refaulting sensor would otherwise cut the
// motor in flight on a protection the pilot knowingly accepted going
// without. A signal valid at arming time that later goes invalid is a
// promise broken mid-flight — that disarms.
//
// This class only decides; it does not gate itself on "are we currently
// armed" or on which task is asking. shouldLimit() is safe to call from
// anywhere (pure read). shouldDisarmOnLoss()'s result must only be acted on
// from the main loop task — see Power::checkSignalLoss(). onArmed() is
// called exactly once per arm, taking a fresh snapshot each time.
class SignalArmContract {
public:
    SignalArmContract() : validAtArm_(false) {}

    // Call once, at the moment arming succeeds, with the signal's current
    // validity.
    void onArmed(bool validNow) { validAtArm_ = validNow; }

    // True if this signal should currently be used for power limiting.
    bool shouldLimit(bool validNow) const { return validAtArm_ && validNow; }

    // True if a disarm should fire right now (valid at arm, invalid now).
    bool shouldDisarmOnLoss(bool validNow) const { return validAtArm_ && !validNow; }

private:
    bool validAtArm_;
};
