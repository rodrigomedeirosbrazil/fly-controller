#ifndef REMOTE_LINK_LOGIC_H
#define REMOTE_LINK_LOGIC_H

#include <stdint.h>

// Hybrid link-loss failsafe thresholds (wireless mode only).
#define FAILSAFE_RAMP_MS   500   // no packet this long -> ramp throttle to zero
#define FAILSAFE_DISARM_MS 3000  // no packet this long -> disarm

enum class FailsafeAction {
    None,        // link healthy (or wired mode)
    RampToZero,  // force 0% throttle; existing ramp-down smooths it; stay armed
    Disarm,      // disarm entirely
};

// Decide the failsafe action from the link state. nowMs/lastRxMs are millis().
inline FailsafeAction computeFailsafe(bool wireless, uint32_t lastRxMs, uint32_t nowMs) {
    if (!wireless) return FailsafeAction::None;
    uint32_t gap = nowMs - lastRxMs;
    if (gap > FAILSAFE_DISARM_MS) return FailsafeAction::Disarm;
    if (gap > FAILSAFE_RAMP_MS)   return FailsafeAction::RampToZero;
    return FailsafeAction::None;
}

#endif // REMOTE_LINK_LOGIC_H
