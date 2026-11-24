#pragma once

#include <stdint.h>

// Rate limiter for the temperature power ceiling.
//
// The linear temperature curves follow the sensor directly, so a reading that
// wobbles by a degree or two pumps the throttle ceiling up and down, and
// integer-degree steps become audible steps in power (seen in flight log
// 20261122_003.csv). The ceiling therefore moves toward its target at a
// bounded rate: fast down (protection), slow up (comfort).
//
// The slow up-rate is also the release when the temperature sensor stops
// being valid: the caller feeds a 100 % target and the ceiling climbs back
// at THERMAL_CEILING_UP_PCT_PER_S instead of jumping to full power. If the
// sensor returns mid-climb, the ceiling falls from where it got to.
//
// The first update after construction or reset() seeds the ceiling to the
// target, so a reboot in flight with a hot motor starts at the real limit
// rather than at 100 % ramping down.
//
// Pure logic, no Arduino dependencies: host-tested in
// test/ThermalCeilingSlewTest.cpp.

constexpr uint32_t THERMAL_CEILING_DOWN_PCT_PER_S = 10;
constexpr uint32_t THERMAL_CEILING_UP_PCT_PER_S = 2;

class ThermalCeilingSlew {
public:
    // Returns the applied ceiling, 0-100. A target above 100 is clamped.
    unsigned int update(unsigned int targetPct, uint32_t nowMs) {
        if (targetPct > PERCENT_MAX) {
            targetPct = PERCENT_MAX;
        }
        const int32_t targetMilli = (int32_t)targetPct * MILLI_PER_PERCENT;

        if (!seeded_) {
            seeded_ = true;
            appliedMilli_ = targetMilli;
            lastMs_ = nowMs;
            return targetPct;
        }

        // Unsigned subtraction keeps this correct across a millis() rollover.
        const uint32_t elapsedMs = nowMs - lastMs_;
        lastMs_ = nowMs;

        // rate in %/s equals milli-percent per ms.
        if (targetMilli < appliedMilli_) {
            appliedMilli_ = stepToward(appliedMilli_, targetMilli,
                                       THERMAL_CEILING_DOWN_PCT_PER_S, elapsedMs);
        } else if (targetMilli > appliedMilli_) {
            appliedMilli_ = stepToward(appliedMilli_, targetMilli,
                                       THERMAL_CEILING_UP_PCT_PER_S, elapsedMs);
        }

        return (unsigned int)((appliedMilli_ + MILLI_PER_PERCENT / 2) / MILLI_PER_PERCENT);
    }

    void reset() {
        seeded_ = false;
    }

private:
    static constexpr unsigned int PERCENT_MAX = 100;
    static constexpr int32_t MILLI_PER_PERCENT = 1000;

    static int32_t stepToward(int32_t from, int32_t to, uint32_t ratePctPerS, uint32_t elapsedMs) {
        const int64_t maxStep = (int64_t)ratePctPerS * elapsedMs;
        const int64_t distance = from > to ? (int64_t)from - to : (int64_t)to - from;
        const int64_t step = distance < maxStep ? distance : maxStep;
        return (int32_t)(from > to ? from - step : from + step);
    }

    bool seeded_ = false;
    int32_t appliedMilli_ = 0;
    uint32_t lastMs_ = 0;
};
