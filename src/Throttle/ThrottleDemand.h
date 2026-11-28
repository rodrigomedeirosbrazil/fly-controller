#pragma once

#include <stdint.h>
#include "ThrottleEngagementLogic.h"

// Pure pilot-demand mapping — no Arduino deps, host-testable.
//
// Maps the filtered throttle reading to a demand in parts per million. Demand is
// 0 until the engage point (the same integer threshold ThrottleEngagementLogic
// uses) and rises linearly to DEMAND_FULL_PPM at DEMAND_TOP_PERCENT of the
// calibrated range, so there is no jump when the gate engages and full demand is
// reachable without squeezing the lever to its mechanical end.

constexpr uint32_t DEMAND_FULL_PPM = 1000000;
constexpr int DEMAND_TOP_PERCENT = 97;

inline uint32_t throttleDemandPpm(int filtered, int pinMin, int pinMax, bool engaged) {
    if (!engaged) return 0;
    const int range = pinMax - pinMin;
    if (range <= 0) return 0;

    const int engagePoint = pinMin + (range * ThrottleEngagementLogic::ENGAGE_THRESHOLD_PERCENT) / 100;
    const int topPoint    = pinMin + (range * DEMAND_TOP_PERCENT) / 100;
    if (topPoint <= engagePoint) return 0;

    if (filtered <= engagePoint) return 0;
    if (filtered >= topPoint) return DEMAND_FULL_PPM;

    return (uint32_t)(((uint64_t)(filtered - engagePoint) * DEMAND_FULL_PPM) /
                      (uint32_t)(topPoint - engagePoint));
}

// Rounded percentage 0..100 of a demand in ppm.
inline unsigned int demandPercent(uint32_t demandPpm) {
    unsigned int p = (demandPpm + 5000) / 10000;
    return p > 100 ? 100 : p;
}
