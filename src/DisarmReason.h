// src/DisarmReason.h
#pragma once
#include <stdint.h>

// Why the system disarmed. One definition, shared by BLE telemetry,
// the CSV log, and Xctod's system-status field — so a reason never drifts
// out of sync between surfaces.
//
// MotorTempLost, EscTempLost and MotorTempSourceChanged are RETIRED: a lost
// temperature sensor no longer disarms (see TempProtectionAlert). Their values
// and codes stay because the number is the wire contract with fly-app and
// already-recorded CSV logs still contain the codes; never reuse them.
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
    MotorTempLost = 4,        // retired, never emitted
    EscTempLost = 5,          // retired, never emitted
    BatteryVoltageLost = 6,
    MotorTempSourceChanged = 7, // retired, never emitted
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
