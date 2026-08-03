#pragma once
#include <stdint.h>
#include <stdbool.h>

// Fires immediately when `active` becomes true, then again every intervalMs
// while it stays true. Resets when `active` goes false.
//
// Time comparisons use unsigned subtraction (nowMs - lastFireMs_), which
// yields the correct elapsed duration even when millis() wraps past
// UINT32_MAX (~49 days uptime) -- unsigned wraparound arithmetic is exact
// as long as the real elapsed time fits in 32 bits.
class PeriodicTrigger {
public:
    PeriodicTrigger() : wasActive_(false), lastFireMs_(0) {}

    bool update(bool active, uint32_t nowMs, uint32_t intervalMs) {
        if (!active) {
            wasActive_ = false;
            lastFireMs_ = 0;
            return false;
        }

        if (!wasActive_) {
            wasActive_ = true;
            lastFireMs_ = nowMs;
            return true;
        }

        if (nowMs - lastFireMs_ >= intervalMs) {
            lastFireMs_ = nowMs;
            return true;
        }

        return false;
    }

private:
    bool     wasActive_;
    uint32_t lastFireMs_;
};
