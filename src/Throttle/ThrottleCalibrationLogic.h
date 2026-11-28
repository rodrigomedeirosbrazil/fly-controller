#pragma once

#include <stdint.h>

// Pure throttle calibration acceptance logic — no Arduino deps, host-testable.
//
// Two-step relative calibration: step 0 captures the squeezed (max) position,
// step 1 captures the released (min) position. Each capture is a "hold": the
// reading must stay within STABILITY_BAND of the hold's first sample for
// HOLD_MS, and the captured value is the mean of the hold's samples.
//
// Step 0 only accepts a hold at least MIN_SPAN above the lowest reading seen
// since reset, so an untouched throttle never captures "max" and stays silent
// (no rest reference needs to exist beforehand: the lowest sample is enough).
// A throttle held open at power-on is never accepted until it has been
// released once and squeezed again. Step 1 rejects captures that are not far
// enough from max (MIN_SPAN): still at max restarts silently, a partial
// release restarts with a beep. Squeezing again during step 1 re-captures max.

enum class CalibrationEvent : uint8_t {
    None,
    MaxCaptured,
    Complete,
    Restart,
    RestartWithBeep
};

class ThrottleCalibrationLogic {
public:
    enum : int { STABILITY_BAND = 40, MIN_SPAN = 600, RESQUEEZE_MARGIN = 80 };
    enum : uint32_t { HOLD_MS = 3000 };

    CalibrationEvent update(int filtered, uint32_t nowMs) {
        if (calibrated_) return CalibrationEvent::None;
        if (filtered < lowest_) lowest_ = filtered;

        if (step_ == 1 && filtered > pinMax_ + RESQUEEZE_MARGIN) {
            step_ = 0;
            startHold(filtered, nowMs);
            return CalibrationEvent::None;
        }

        if (!holding_ || abs32(filtered - anchor_) > STABILITY_BAND) {
            startHold(filtered, nowMs);
            return CalibrationEvent::None;
        }

        sum_ += (uint32_t)filtered;
        count_++;
        if ((uint32_t)(nowMs - holdStartMs_) < HOLD_MS) return CalibrationEvent::None;

        const int mean = (int)(sum_ / count_);
        holding_ = false;

        if (step_ == 0) {
            // Not a squeeze: the throttle was untouched, or held open since
            // power-on (lowest_ is still that position). Stay silent.
            if (mean < lowest_ + MIN_SPAN) return CalibrationEvent::None;
            pinMax_ = mean;
            step_ = 1;
            return CalibrationEvent::MaxCaptured;
        }

        if (mean <= pinMax_ - MIN_SPAN) {
            pinMin_ = mean;
            calibrated_ = true;
            return CalibrationEvent::Complete;
        }

        step_ = 0;
        return abs32(mean - pinMax_) <= STABILITY_BAND
            ? CalibrationEvent::Restart
            : CalibrationEvent::RestartWithBeep;
    }

    void reset() {
        step_ = 0;
        holding_ = false;
        calibrated_ = false;
        pinMin_ = 0;
        pinMax_ = 0;
        sum_ = 0;
        count_ = 0;
        lowest_ = LOWEST_UNSET;
    }

    bool isCalibrated() const { return calibrated_; }
    int pinMin() const { return pinMin_; }
    int pinMax() const { return pinMax_; }
    uint8_t step() const { return step_; }

private:
    enum : int { LOWEST_UNSET = 0x7FFFFFFF - MIN_SPAN };
    static int abs32(int v) { return v < 0 ? -v : v; }

    void startHold(int value, uint32_t nowMs) {
        holding_ = true;
        anchor_ = value;
        holdStartMs_ = nowMs;
        sum_ = (uint32_t)value;
        count_ = 1;
    }

    uint8_t step_ = 0;
    bool holding_ = false;
    bool calibrated_ = false;
    int anchor_ = 0;
    int pinMin_ = 0;
    int pinMax_ = 0;
    int lowest_ = LOWEST_UNSET;
    uint32_t holdStartMs_ = 0;
    uint32_t sum_ = 0;
    uint32_t count_ = 0;
};
