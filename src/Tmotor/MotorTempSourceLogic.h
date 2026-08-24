#pragma once
#include <stdint.h>
#include "../Telemetry/SignalState.h"

// Which sensor actually fed a motor-temperature reading. None exists only
// for builds/backends that don't select a motor-temp source at all (XAG has
// a single source; the source badge is omitted there).
enum class MotorTempOrigin : uint8_t { None, Can, Ntc };

struct MotorTempReading {
    int32_t milliCelsius;
    SignalState state;
    MotorTempOrigin origin;
};

// Pure motor-temperature source-selection decision — no Arduino deps,
// host-testable. See docs/superpowers/specs/2026-08-01-signal-validity-design.md.
//
// Motor temp has two redundant sources on Tmotor: CAN (Status 5/PUSHCAN) and
// a fallback NTC thermistor. The value, state, AND origin returned must
// always come from the SAME source — returning one source's number next to a
// different source's health verdict was a real bug this function fixes
// structurally: there are exactly two return sites, each pairing a source's
// value, validity, and origin in a single literal, so there is no path that
// can desync them.
//
// `canFreshAndPlausible` is the caller's already-computed verdict on the CAN
// reading (fresh within the last second AND within the physically plausible
// range) — the freshness/plausibility check itself touches TmotorCan and
// stays in the Arduino-coupled caller; this function only decides which
// source wins once that verdict is known.
inline MotorTempReading selectMotorTempReading(
    bool forceNtc,
    bool canFreshAndPlausible,
    int32_t canMilliCelsius,
    double ntcCelsius,
    bool ntcValid) {
    if (!forceNtc && canFreshAndPlausible) {
        return { canMilliCelsius, SignalState::Valid, MotorTempOrigin::Can };
    }
    return {
        (int32_t)(ntcCelsius * 1000.0),
        ntcValid ? SignalState::Valid : SignalState::Invalid,
        MotorTempOrigin::Ntc
    };
}
