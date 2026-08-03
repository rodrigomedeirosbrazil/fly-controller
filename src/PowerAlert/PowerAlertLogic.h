#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "../Sound/PeriodicTrigger.h"

// Pure power-alert timing logic -- no Arduino deps, host-testable.
// Call update() each loop tick; returns true when a beep should be emitted.
//
// Fires immediately on the transition into the limited state, then once every
// intervalMs while it persists. Resets when limited is no longer true.
//
// A thin wrapper over the general-purpose PeriodicTrigger (see Sound/), which
// the wireless link-loss warning also uses.
class PowerAlertLogic {
public:
    bool update(bool armed, uint8_t activeCauses, uint32_t nowMs, uint32_t intervalMs) {
        bool limited = armed && (activeCauses != 0);
        return trigger_.update(limited, nowMs, intervalMs);
    }

private:
    PeriodicTrigger trigger_;
};
