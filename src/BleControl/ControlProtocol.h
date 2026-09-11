#ifndef CONTROL_PROTOCOL_H
#define CONTROL_PROTOCOL_H

#include <stdint.h>
#include <string.h>

// Wire contract for the Fly Control GATT service. Pure: no Arduino, no
// hardware, host-testable.
//
// This header is duplicated as Dart in the fly-app repo. There is no shared
// package and no runtime negotiation -- bump CONTROL_PROTOCOL_VERSION in both
// repos whenever an existing field changes position or meaning, and update
// both tests. See docs in CLAUDE.md ("BLE Control Service").
#define CONTROL_PROTOCOL_VERSION 1

#define BLE_CONTROL_SERVICE_UUID   "D4CF0001-9B9D-4BFD-8F7F-40C6989D3EA9"
#define BLE_CONTROL_INFO_UUID      "D4CF0002-9B9D-4BFD-8F7F-40C6989D3EA9"
#define BLE_CONTROL_TELEMETRY_UUID "D4CF0003-9B9D-4BFD-8F7F-40C6989D3EA9"
#define BLE_CONTROL_CMD_UUID       "D4CF0004-9B9D-4BFD-8F7F-40C6989D3EA9"
#define BLE_CONTROL_RSP_UUID       "D4CF0005-9B9D-4BFD-8F7F-40C6989D3EA9"

// A 247-byte MTU leaves 244 usable bytes. Everything in this phase fits in a
// single notification, so there is no fragmentation layer.
#define CONTROL_MAX_FRAME   244
#define CONTROL_MAX_PAYLOAD (CONTROL_MAX_FRAME - 4)

// seq 0 on RSP means "unsolicited event", never a reply to a request. Request
// senders must therefore start at 1.
#define CONTROL_EVENT_SEQ 0

namespace ControlOp {
    enum : uint8_t {
        Auth             = 0x01,

        CfgGet           = 0x10,
        CfgSet           = 0x11,

        SessionReset     = 0x20,
        BmsScanStart     = 0x21,
        BmsScanStatus    = 0x22,
        BmsDetect        = 0x23,
        RemotePair       = 0x24,
        RemoteForget     = 0x25,
        BuzzerPreview    = 0x26,
        SetTime          = 0x27,
        PinChange        = 0x28,
        TmotorDirForward = 0x29,
        TmotorDirReverse = 0x2A,

        // 0x40-0x4F reserved for log download, 0x50-0x5F for DFU (phase 3).

        EvtBeep          = 0x80,
    };
}

enum class ControlStatus : uint8_t {
    Ok       = 0,
    ErrAuth  = 1,  // PIN not presented on this connection
    ErrBadOp = 2,  // unknown opcode: lets a newer app degrade instead of hang
    ErrBadArg = 3, // payload malformed or out of range
    ErrState = 4,  // refused in the current state (armed)
    ErrBusy  = 5,  // another long-running operation holds the resource
};

enum class ConfigGroup : uint8_t {
    Power   = 0,
    Thermal = 1,
    Bms     = 2,
    System  = 3,
};

// CMD frame: [op][seq][len][payload len]
struct ControlRequest {
    uint8_t        op;
    uint8_t        seq;
    uint8_t        len;
    const uint8_t* payload;
};

inline bool decodeRequest(const uint8_t* buf, size_t n, ControlRequest& out) {
    if (buf == nullptr || n < 3) {
        return false;
    }
    const uint8_t len = buf[2];
    if (n < (size_t) 3 + len || len > CONTROL_MAX_PAYLOAD) {
        return false;
    }
    out.op      = buf[0];
    out.seq     = buf[1];
    out.len     = len;
    out.payload = buf + 3;
    return true;
}

// RSP frame: [op][seq][status][len][payload len]. Returns the frame length, or
// 0 when it does not fit -- never a truncated frame, which the app would
// decode as valid and short.
inline size_t encodeResponse(uint8_t* buf, size_t cap, uint8_t op, uint8_t seq,
                             ControlStatus status, const uint8_t* payload,
                             uint8_t len) {
    if (buf == nullptr || len > CONTROL_MAX_PAYLOAD || cap < (size_t) 4 + len) {
        return 0;
    }
    // A caller promising len bytes but passing no buffer would otherwise ship
    // len bytes of uninitialised stack over the air under a header that
    // decodes cleanly. Fail like every other bad-argument case here.
    if (len > 0 && payload == nullptr) {
        return 0;
    }
    buf[0] = op;
    buf[1] = seq;
    buf[2] = (uint8_t) status;
    buf[3] = len;
    if (len > 0) {
        memcpy(buf + 4, payload, len);
    }
    return (size_t) 4 + len;
}

// ---------------------------------------------------------------------------
// INFO characteristic payload
//
// Static for the whole session: written once at init(), then read by the
// central whenever it likes. Decoded by hand in the fly-app repo just like
// ControlTelemetry, so its layout is pinned by a test for the same reason.
// ---------------------------------------------------------------------------

namespace Capability {
    enum : uint16_t {
        CanTelemetry       = 1 << 0,
        VoltageSensor      = 1 << 1,
        MotorTempSourceSel = 1 << 2,
        RemoteLink         = 1 << 3,
    };
}

#pragma pack(push, 1)
struct ControlInfo {
    uint8_t  protocolVersion;
    uint8_t  controllerType;
    uint16_t capabilities;
    char     appVersion[24];   // NUL-padded
};
#pragma pack(pop)

// ---------------------------------------------------------------------------
// Telemetry
//
// Two rules, and they are the whole contract:
//
//  1. Fixed layout. An unavailable field is zero with its validity bit clear,
//     never an absent field -- so every offset is constant forever. This is
//     what the $XCTOD CSV could not do.
//  2. Append only at the end, and the reader parses min(received, known).
//     New firmware + old app: the app reads the prefix it understands. Old
//     firmware + new app: the app sees a short packet. Neither breaks.
//
// `ver` is bumped ONLY if an existing field changes position or meaning.
// Appending a field at the end does not bump it.
// ---------------------------------------------------------------------------

namespace TelemFlag {
    enum : uint8_t {
        Armed               = 1 << 0,
        Engaged             = 1 << 1,  // throttle past the engage hysteresis
        HasTelemetry        = 1 << 2,
        PowerControlEnabled = 1 << 3,
        BmsConnected        = 1 << 4,
        BmsConfigured       = 1 << 5,
    };
}

// Availability: does this build and configuration produce this reading at
// all? That is a different question from sensor health, which `signalStates`
// answers with the firmware's own four-state SignalState.
//
// Motor temp, ESC temp and battery voltage deliberately have NO bit here.
// Telemetry::isMotorTempValid() and its siblings are literally
// `state == SignalState::Valid`, so a bit would be derived duplication --
// two answers to one question in the same packet, populated from two call
// sites and free to drift. The app reads signalStates for those three.
//
// SoC has no bit either, because the firmware has no validity concept for
// it: socVolt's trustworthiness follows the battery-voltage SignalState and
// coulomb counting has none. A bit that is always 1 is worse than no bit.
// This mask is a uint16_t with 11 spare, and adding one later is not a
// breaking change, so there is nothing to reserve now.
namespace TelemValid {
    enum : uint16_t {
        Current   = 1 << 0,
        Rpm       = 1 << 1,
        PowerKw   = 1 << 2,
        Bms       = 1 << 3,
        BmsCells  = 1 << 4,
    };
}

#pragma pack(push, 1)
struct ControlTelemetry {
    uint8_t  ver;
    uint8_t  flags;          // TelemFlag bitmask
    uint16_t validity;       // TelemValid bitmask (availability, not health)
    uint8_t  disarmReason;   // enum DisarmReason (src/DisarmReason.h)
    uint8_t  signalStates;   // packSignalStates(motorTemp, escTemp, battV)
    uint8_t  motorTempSrc;   // enum MotorTempOrigin
    uint8_t  socCc;          // %
    uint8_t  socVolt;        // %
    uint8_t  throttlePct;
    uint8_t  powerPct;       // available power
    uint8_t  powerScale;     // disarm ramp scale
    uint8_t  armCharge;
    uint8_t  limitCauses;    // POWER_LIMIT_* bitmask (src/Power/Power.h)
    uint16_t batteryMv;
    uint16_t throttleRaw;
    uint16_t powerKwX10;
    int32_t  escCurrentMa;   // signed: regen is a legitimate reading
    uint32_t rpm;
    int32_t  motorTempMc;    // millicelsius, the firmware's unit everywhere
    int32_t  escTempMc;
    uint32_t sessionSec;
    uint32_t hourMeterSec;
    uint16_t bmsCellMinMv;
    uint16_t bmsCellMaxMv;
    uint16_t bmsCellDeltaMv;
    int16_t  bmsTempMaxC;
    uint32_t uptimeSec;
};
#pragma pack(pop)

// SignalState is 0..3 (Absent/Stale/Invalid/Valid), so three signals fit in
// one byte with two bits each.
inline uint8_t packSignalStates(uint8_t motorTemp, uint8_t escTemp, uint8_t battV) {
    return (uint8_t) (((motorTemp & 0x03) << 4) |
                      ((escTemp   & 0x03) << 2) |
                       (battV     & 0x03));
}
inline uint8_t unpackMotorTempState(uint8_t packed) { return (packed >> 4) & 0x03; }
inline uint8_t unpackEscTempState(uint8_t packed)   { return (packed >> 2) & 0x03; }
inline uint8_t unpackBatteryVState(uint8_t packed)  { return  packed       & 0x03; }

// The append rule, in one function. `dst` must already hold the reader's
// current values: the prefix the sender knows is overlaid on top, and
// anything the sender did not send keeps what was already there. Returns the
// number of bytes copied.
inline size_t copyKnownPrefix(void* dst, size_t dstSize, const void* src, size_t srcSize) {
    if (dst == nullptr || src == nullptr) {
        return 0;
    }
    const size_t n = dstSize < srcSize ? dstSize : srcSize;
    memcpy(dst, src, n);
    return n;
}

#endif // CONTROL_PROTOCOL_H
