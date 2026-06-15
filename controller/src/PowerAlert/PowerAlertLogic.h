#pragma once
#include <stdint.h>
#include <stdbool.h>

// Pure power-alert timing logic — no Arduino deps, host-testable.
// Call update() each loop tick; returns true when a beep should be emitted.
//
// Fires immediately on the transition into the limited state, then once every
// intervalMs while it persists. Resets when limited is no longer true.
class PowerAlertLogic {
public:
    PowerAlertLogic() : wasLimited_(false), lastBeepMs_(0) {}

    bool update(bool armed, uint8_t activeCauses, uint32_t nowMs, uint32_t intervalMs) {
        bool limited = armed && (activeCauses != 0);

        if (!limited) {
            wasLimited_ = false;
            lastBeepMs_ = 0;
            return false;
        }

        if (!wasLimited_) {
            wasLimited_ = true;
            lastBeepMs_ = nowMs;
            return true;
        }

        if (nowMs - lastBeepMs_ >= intervalMs) {
            lastBeepMs_ = nowMs;
            return true;
        }

        return false;
    }

private:
    bool     wasLimited_;
    uint32_t lastBeepMs_;
};
