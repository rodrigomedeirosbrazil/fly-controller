#pragma once

#include <stdint.h>

// Pure throttle output stage — no Arduino deps, host-testable.
//
// Turns the pilot demand into the motor output (both in ppm of the range,
// 0..OUTPUT_FULL_PPM): soft start from stopped, instant response while running,
// a running offset, the limiter ceiling, and a downward-only decel limit that
// applies to every drop, disarm included. See the throttle output stage spec.

constexpr uint32_t OUTPUT_FULL_PPM = 1000000;

struct ThrottleOutputConfig {
    bool     softStart;
    uint32_t wakeWaitMs;            // output held at <= wakeLevelPpm this long after entering Starting
    uint32_t wakeLevelPpm;
    uint32_t startRampTopPpm;
    uint32_t startRampRatePpmPerS;  // upward slew in Starting, from the current output
    bool     releaseByRpm;
    uint16_t releaseRpm;
    uint32_t releaseMaxMs;          // time fallback, from entering Starting; only once output >= ramp top
    uint32_t runningOffsetPpm;
    uint32_t decelRatePpmPerS;
    bool     stopByRpm;
    uint32_t rpmStopConfirmMs;
    uint32_t coastStopMs;           // stop rule without (fresh) RPM
};

struct ThrottleOutputInput {
    uint32_t nowMs;
    bool calibrated;
    bool armed;
    uint32_t demandPpm;
    uint32_t ceilingPpm;
    uint8_t disarmScalePct;
    bool rpmFresh;
    uint16_t rpm;
};

enum class ThrottleOutputState : uint8_t { Stopped, Starting, Running };

class ThrottleOutputLogic {
public:
    explicit ThrottleOutputLogic(const ThrottleOutputConfig& cfg) : cfg_(cfg) {}

    uint32_t update(const ThrottleOutputInput& in) {
        const uint32_t dt = first_ ? 0 : (uint32_t)(in.nowMs - lastMs_);
        first_ = false;
        lastMs_ = in.nowMs;
        clipped_ = false;

        if (!in.calibrated) {
            output_ = 0;
            state_ = ThrottleOutputState::Stopped;
            resetStopTimers();
            return output_;
        }

        uint32_t target = 0;
        if (!in.armed) {
            state_ = ThrottleOutputState::Stopped;
            resetStopTimers();
        } else {
            transition(in);
            target = computeTarget(in, dt);

            const uint32_t pre = target;
            if (target > in.ceilingPpm) target = in.ceilingPpm;
            clipped_ = in.demandPpm > 0 && target < pre;
            target = (uint32_t)(((uint64_t)target * in.disarmScalePct) / 100);
        }

        if (target < output_) {
            const uint32_t step = (uint32_t)(((uint64_t)cfg_.decelRatePpmPerS * dt) / 1000);
            const uint32_t floorV = output_ > step ? output_ - step : 0;
            output_ = floorV > target ? floorV : target;
        } else {
            output_ = target;
        }
        return output_;
    }

    uint32_t output() const { return output_; }
    ThrottleOutputState state() const { return state_; }
    bool isClipped() const { return clipped_; }
    bool isAboveMinimum() const { return output_ > 0; }

private:
    void resetStopTimers() { rpmZeroActive_ = false; zeroActive_ = false; }

    void transition(const ThrottleOutputInput& in) {
        switch (state_) {
            case ThrottleOutputState::Stopped:
                if (in.demandPpm > 0) {
                    state_ = cfg_.softStart ? ThrottleOutputState::Starting
                                            : ThrottleOutputState::Running;
                    startedAt_ = in.nowMs;
                    resetStopTimers();
                }
                break;
            case ThrottleOutputState::Starting:
                if (in.demandPpm == 0) {
                    state_ = ThrottleOutputState::Stopped;
                    break;
                }
                if ((uint32_t)(in.nowMs - startedAt_) >= cfg_.wakeWaitMs) {
                    const bool byRpm = cfg_.releaseByRpm && in.rpmFresh && in.rpm >= cfg_.releaseRpm;
                    const bool byTime = output_ >= cfg_.startRampTopPpm &&
                                        (uint32_t)(in.nowMs - startedAt_) >= cfg_.releaseMaxMs;
                    if (byRpm || byTime) {
                        state_ = ThrottleOutputState::Running;
                        resetStopTimers();
                    }
                }
                break;
            case ThrottleOutputState::Running:
                if (in.demandPpm > 0 || output_ != 0) {
                    resetStopTimers();
                    break;
                }
                if (cfg_.stopByRpm && in.rpmFresh) {
                    zeroActive_ = false;
                    if (in.rpm != 0) {
                        rpmZeroActive_ = false;
                    } else {
                        if (!rpmZeroActive_) { rpmZeroActive_ = true; rpmZeroSince_ = in.nowMs; }
                        if ((uint32_t)(in.nowMs - rpmZeroSince_) >= cfg_.rpmStopConfirmMs)
                            state_ = ThrottleOutputState::Stopped;
                    }
                } else {
                    rpmZeroActive_ = false;
                    if (!zeroActive_) { zeroActive_ = true; zeroSince_ = in.nowMs; }
                    if ((uint32_t)(in.nowMs - zeroSince_) >= cfg_.coastStopMs)
                        state_ = ThrottleOutputState::Stopped;
                }
                break;
        }
    }

    uint32_t computeTarget(const ThrottleOutputInput& in, uint32_t dt) const {
        switch (state_) {
            case ThrottleOutputState::Running:
                if (in.demandPpm == 0) return 0;
                return cfg_.runningOffsetPpm +
                       (uint32_t)(((uint64_t)(OUTPUT_FULL_PPM - cfg_.runningOffsetPpm) * in.demandPpm) /
                                  OUTPUT_FULL_PPM);
            case ThrottleOutputState::Starting: {
                uint32_t cap;
                if ((uint32_t)(in.nowMs - startedAt_) < cfg_.wakeWaitMs) {
                    cap = cfg_.wakeLevelPpm;
                } else {
                    const uint32_t base = output_ > cfg_.wakeLevelPpm ? output_ : cfg_.wakeLevelPpm;
                    const uint64_t up = (uint64_t)base + ((uint64_t)cfg_.startRampRatePpmPerS * dt) / 1000;
                    cap = up < cfg_.startRampTopPpm ? (uint32_t)up : cfg_.startRampTopPpm;
                }
                return in.demandPpm < cap ? in.demandPpm : cap;
            }
            case ThrottleOutputState::Stopped:
            default:
                return 0;
        }
    }

    ThrottleOutputConfig cfg_;
    uint32_t output_ = 0;
    uint32_t lastMs_ = 0;
    uint32_t startedAt_ = 0;
    uint32_t rpmZeroSince_ = 0;
    uint32_t zeroSince_ = 0;
    bool first_ = true;
    bool clipped_ = false;
    bool rpmZeroActive_ = false;
    bool zeroActive_ = false;
    ThrottleOutputState state_ = ThrottleOutputState::Stopped;
};

// Maps an output in ppm of the range onto the ESC pulse width, rounded.
inline uint16_t outputToPwm(uint32_t outputPpm, uint16_t escMinPwm, uint16_t escMaxPwm) {
    if (outputPpm > OUTPUT_FULL_PPM) outputPpm = OUTPUT_FULL_PPM;
    const uint32_t span = escMaxPwm > escMinPwm ? (uint32_t)(escMaxPwm - escMinPwm) : 0;
    return (uint16_t)(escMinPwm + (((uint64_t)span * outputPpm + OUTPUT_FULL_PPM / 2) / OUTPUT_FULL_PPM));
}
