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
// "recovered" on every good packet and never reach Disarm. Note that the
// elapsed-since-first-invalid clock is NOT paused while a recovery run is
// pending — it keeps advancing in the background — but `update()` never
// returns Disarm while the current sample is valid; see the downgrade below.
struct ThrottleSignalConfig {
    uint32_t debounceMs; // invalid duration before ForceZero
    uint32_t disarmMs;   // invalid duration before Disarm (must be >= debounceMs)
    uint32_t recoveryMs; // consecutive valid duration before the episode clears
};

// Ok: signal trustworthy. ForceZero: hold output at zero, still armed.
// Disarm: latch a full disarm — this is an idempotent level, not an edge:
// callers will see it returned on every tick the fault persists, not just
// the first.
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
            ThrottleSignalAction action = actionForElapsed(nowMs - firstInvalidMs_, cfg);
            if (action == ThrottleSignalAction::Disarm) {
                // Never disarm on a tick where the current sample is valid —
                // the elapsed-since-first-invalid clock can cross disarmMs
                // while a recovery run is still in progress (hasn't hit
                // recoveryMs yet). The signal is fine right now; ForceZero
                // (not Ok, since we haven't confirmed recovery) is the
                // correct action, not Disarm.
                action = ThrottleSignalAction::ForceZero;
            }
            return action;
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
    // Assumes cfg.disarmMs >= cfg.debounceMs. Violating that invariant makes
    // ForceZero unreachable for this config — Disarm wins in the `if`
    // ordering below since elapsedMs crosses disarmMs before it ever gets a
    // chance to be checked against the (larger) debounceMs.
    static ThrottleSignalAction actionForElapsed(uint32_t elapsedMs, const ThrottleSignalConfig& cfg) {
        if (elapsedMs >= cfg.disarmMs)   return ThrottleSignalAction::Disarm;
        if (elapsedMs >= cfg.debounceMs) return ThrottleSignalAction::ForceZero;
        return ThrottleSignalAction::Ok;
    }

    uint32_t firstInvalidMs_;
    uint32_t validRunStartMs_;
    bool     hasInvalidEpisode_;
};
