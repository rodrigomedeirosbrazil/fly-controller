#pragma once
#include <stdint.h>

// Where a motor-temperature reading actually came from. One definition,
// shared by the /api/telemetry `signals.motorTempSrc` field and the
// Telemetry page's source badge — so the origin never drifts out of sync
// between the two surfaces.
//
// `None` is a sentinel for "no source choice to report", not a real source:
// XAG has a single motor-temp source (NTC/ADS1115) and reports None; a
// Tmotor reading that hasn't been produced yet (pre-first-update) still
// reads None; and a null telemetry backend reports None too. The web page
// shows a source badge only for Can/Ntc.
enum class MotorTempOrigin : uint8_t { None, Can, Ntc };

// JSON/UI code for a source origin. nullptr means "no source to report" —
// the caller omits the field (matching how rpm/current are omitted) rather
// than emitting a meaningless value.
inline const char* motorTempOriginCode(MotorTempOrigin origin) {
    switch (origin) {
        case MotorTempOrigin::Can:  return "can";
        case MotorTempOrigin::Ntc:  return "ntc";
        case MotorTempOrigin::None: return nullptr;
    }
    return nullptr;
}
