#pragma once
#include <stdint.h>

// Four-state signal validity model — see
// docs/superpowers/specs/2026-08-01-signal-validity-design.md.
//   Absent  — the source doesn't exist in this build (e.g. RPM on XAG)
//   Stale   — the source exists but hasn't delivered recently
//   Invalid — a reading arrived but is physically impossible
//   Valid   — trustworthy; zero is a legitimate value
enum class SignalState : uint8_t { Absent, Stale, Invalid, Valid };

// Single-letter code used identically across /api/telemetry's `signals`
// object and (later) the CSV log.
inline char signalStateCode(SignalState s) {
    switch (s) {
        case SignalState::Absent:  return 'a';
        case SignalState::Stale:   return 's';
        case SignalState::Invalid: return 'i';
        case SignalState::Valid:   return 'v';
    }
    return 'a';
}
