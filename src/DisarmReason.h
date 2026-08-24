// src/DisarmReason.h
#pragma once
#include <stdint.h>

// Why the system disarmed. One definition, shared by the telemetry web page,
// the CSV log, and Xctod's system-status field (phase 2) — so a reason never
// drifts out of sync between surfaces.
//
// `None` is an internal sentinel only: the boot-time default, and what
// Throttle resets to on a successful arm. It is never meant to be shown next
// to an armed system; it exists so "never disarmed yet" is distinguishable
// from "disarmed manually".
enum class DisarmReason : uint8_t {
    None = 0,
    Manual = 1,
    ThrottleWiredInvalid = 2,
    ThrottleLinkLost = 3,
    MotorTempLost = 4,        // phase 2
    EscTempLost = 5,          // phase 2
    BatteryVoltageLost = 6,   // phase 2
    MotorTempSourceChanged = 7, // phase 2
};

// Short fixed-width code (<= 8 chars), identical across every surface that
// shows a disarm reason.
inline const char* disarmReasonCode(DisarmReason reason) {
    switch (reason) {
        case DisarmReason::None:                 return "";
        case DisarmReason::Manual:                return "MANUAL";
        case DisarmReason::ThrottleWiredInvalid:  return "THR ERR";
        case DisarmReason::ThrottleLinkLost:      return "LINK ERR";
        case DisarmReason::MotorTempLost:         return "MOT ERR";
        case DisarmReason::EscTempLost:           return "ESC ERR";
        case DisarmReason::BatteryVoltageLost:    return "BATT ERR";
        case DisarmReason::MotorTempSourceChanged: return "MOT SRC";
    }
    return "";
}
