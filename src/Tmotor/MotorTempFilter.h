#pragma once

#include <stdint.h>
#include <math.h>

// Filter for the T-Motor CAN motor temperature (Status 5).
//
// Why it exists: in the flight log of 2026-11-21 the CAN reading carried
// +-7 C of noise under load, and the motor derating is 5 %/C, so every noise
// sample became a 20-40 % power step. A time-based EMA turns that into the
// thermal trend.
//
// Responsibilities:
//  - plausibility per sample: a sample above the ceiling (or below 0) is
//    dropped and changes nothing, including freshness;
//  - freshness: a plausible sample within MOTOR_TEMP_FILTER_FRESH_MS;
//  - smoothing: EMA with tau MOTOR_TEMP_FILTER_TAU_MS, re-seeded when the
//    reading was not fresh (ESC reboot, CAN unplug, a run of bad samples).
//
// SignalState must be derived from isFresh(), never from the filtered value.
//
// Pure and header-only: host-tested in test/MotorTempFilterTest.cpp.

constexpr uint32_t MOTOR_TEMP_FILTER_TAU_MS = 5000;
constexpr uint32_t MOTOR_TEMP_FILTER_FRESH_MS = 1000;

class MotorTempFilter {
public:
    explicit MotorTempFilter(int32_t maxValidMilliCelsius)
        : maxValid(maxValidMilliCelsius) {}

    // Returns false (and changes nothing) for an implausible sample.
    bool addSample(int32_t milliCelsius, uint32_t nowMs) {
        if (milliCelsius > maxValid || milliCelsius < 0) {
            return false;
        }
        if (!isFresh(nowMs)) {
            value = (float)milliCelsius;
        } else {
            uint32_t dt = nowMs - lastAcceptedMs;
            if (dt > 0) {
                float alpha = 1.0f - expf(-(float)dt / (float)MOTOR_TEMP_FILTER_TAU_MS);
                value += alpha * ((float)milliCelsius - value);
            }
        }
        lastAcceptedMs = nowMs;
        hasSample = true;
        return true;
    }

    bool isFresh(uint32_t nowMs) const {
        return hasSample && (uint32_t)(nowMs - lastAcceptedMs) < MOTOR_TEMP_FILTER_FRESH_MS;
    }

    int32_t valueMilliCelsius() const {
        if (!hasSample) return 0;
        return (int32_t)lroundf(value);
    }

    void reset() {
        hasSample = false;
        value = 0.0f;
        lastAcceptedMs = 0;
    }

private:
    int32_t maxValid;
    float value = 0.0f;
    uint32_t lastAcceptedMs = 0;
    bool hasSample = false;
};
