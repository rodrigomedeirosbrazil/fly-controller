// src/Throttle/ThrottleSignalLogic.h
#pragma once
#include <stdint.h>

// Pure throttle-signal validity state machine — no Arduino deps, host-testable.
//
// Feeds a per-tick "is this sample trustworthy" boolean through a
// debounce/disarm timer parameterized per source:
//   - Wired:    debounceMs=0, disarmMs=0 — the first invalid sample disarms.
//     No fallback to a held value: a wired fault does not heal on its own,
//     and riding a stale command while the pilot is unaware is worse than a
//     clean stop they can diagnose on the ground.
//   - Wireless: debounceMs=500, disarmMs=3000 — matches the previous
//     RemoteLinkLogic timings. A radio dropout heals constantly (the pilot
//     turns, someone steps clear of the antenna), so it keeps its existing
//     tolerance via the ForceZero intermediate state.
//
// recoveryMs guards against a flapping signal: the invalid-episode clock only
// resets after recoveryMs of *consecutive* valid samples. Without it, a link
// delivering one isolated good packet every couple of seconds would look
// "recovered" on every good packet and never reach Disarm.
struct ThrottleSignalConfig {
    uint32_t debounceMs; // invalid duration before ForceZero
    uint32_t disarmMs;   // invalid duration before Disarm (must be >= debounceMs)
    uint32_t recoveryMs; // consecutive valid duration before the episode clears
};

enum class ThrottleSignalAction : uint8_t { Ok, ForceZero, Disarm };

class ThrottleSignalLogic {
public:
    ThrottleSignalLogic()
        : firstInvalidMs_(0), validRunStartMs_(0), hasInvalidEpisode_(false) {}

    ThrottleSignalAction update(bool valid, uint32_t nowMs, const ThrottleSignalConfig& cfg) {
        if (valid) {
            if (validRunStartMs_ == 0) {
                validRunStartMs_ = nowMs;
            }
            if (!hasInvalidEpisode_) {
                return ThrottleSignalAction::Ok;
            }
            if (nowMs - validRunStartMs_ >= cfg.recoveryMs) {
                hasInvalidEpisode_ = false;
                validRunStartMs_ = 0;
                return ThrottleSignalAction::Ok;
            }
            return actionForElapsed(nowMs - firstInvalidMs_, cfg);
        }

        validRunStartMs_ = 0;
        if (!hasInvalidEpisode_) {
            hasInvalidEpisode_ = true;
            firstInvalidMs_ = nowMs;
        }
        return actionForElapsed(nowMs - firstInvalidMs_, cfg);
    }

    void reset() {
        firstInvalidMs_ = 0;
        validRunStartMs_ = 0;
        hasInvalidEpisode_ = false;
    }

private:
    static ThrottleSignalAction actionForElapsed(uint32_t elapsedMs, const ThrottleSignalConfig& cfg) {
        if (elapsedMs >= cfg.disarmMs)   return ThrottleSignalAction::Disarm;
        if (elapsedMs >= cfg.debounceMs) return ThrottleSignalAction::ForceZero;
        return ThrottleSignalAction::Ok;
    }

    uint32_t firstInvalidMs_;
    uint32_t validRunStartMs_;
    bool     hasInvalidEpisode_;
};
