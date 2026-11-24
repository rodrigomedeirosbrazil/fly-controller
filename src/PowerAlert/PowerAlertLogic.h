#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "../Sound/PeriodicTrigger.h"

// Pure power-alert timing logic -- no Arduino deps, host-testable.
// Call update() each loop tick; returns true when a beep should be emitted.
//
// The alert is active while armed, at least one limiter has its ceiling below
// 100 %, and the throttle was clipped (pilot demand above the ceiling) within
// the last intervalMs. The hold keeps a pilot hovering at the ceiling, whose
// clipping toggles, from beeping every time it comes back.
//
// Fires immediately on entry, then once every intervalMs while it persists.
// Disarm or no active cause ends it at once and clears the hold.
//
// A thin wrapper over the general-purpose PeriodicTrigger (see Sound/).
class PowerAlertLogic {
public:
    bool update(bool armed, uint8_t activeCauses, bool clipped, uint32_t nowMs, uint32_t intervalMs) {
        bool limited = armed && (activeCauses != 0);
        if (!limited) {
            hasClipped_ = false;
        } else if (clipped) {
            lastClippedMs_ = nowMs;
            hasClipped_ = true;
        }
        bool clippedRecently = clipped || (hasClipped_ && (uint32_t)(nowMs - lastClippedMs_) < intervalMs);
        return trigger_.update(limited && clippedRecently, nowMs, intervalMs);
    }

private:
    PeriodicTrigger trigger_;
    uint32_t lastClippedMs_ = 0;
    bool hasClipped_ = false;
};
