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
#define BLE_CONTROL_DFU_UUID       "D4CF0006-9B9D-4BFD-8F7F-40C6989D3EA9"

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

        // 0x40-0x4F reserved for log download.

        DfuBegin  = 0x50,
        DfuCommit = 0x51,
        DfuAbort  = 0x52,
        DfuStatus = 0x53,

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
    // Appended after the first release -- the append rule in action, so no
    // ver bump and older clients simply never see it.
    //
    // The state-layer tone in Hz, 0 when none is playing. EVT_BEEP fires only
    // on a state transition and carries the catalog pattern, but the arm and
    // disarm gestures sweep between 1800 and 2500 Hz on every on->off edge
    // and push nothing. Without this a client mirrors those gestures flat, or
    // reimplements the sweep arithmetic from main.cpp in another repository
    // with nothing checking the two copies agree.
    uint16_t stateFreqHz;
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

// ---------------------------------------------------------------------------
// Dispatch gate
//
// Two policies, stated once:
//
//  - Auth: reads are open, writes need the PIN -- the same split the web
//    portal has, where every GET is free and every POST calls checkPin().
//  - Armed: refuse by DEFAULT, allow by exception. The thermal POST handler
//    already refuses a motorTempSource change while armed; over BLE, with the
//    phone in a pocket, the same hazard applies to everything that can reach
//    the motor, so the default is inverted rather than enumerated.
// ---------------------------------------------------------------------------

inline bool opIsKnown(uint8_t op) {
    switch (op) {
        case ControlOp::Auth:
        case ControlOp::CfgGet:
        case ControlOp::CfgSet:
        case ControlOp::SessionReset:
        case ControlOp::BmsScanStart:
        case ControlOp::BmsScanStatus:
        case ControlOp::BmsDetect:
        case ControlOp::RemotePair:
        case ControlOp::RemoteForget:
        case ControlOp::BuzzerPreview:
        case ControlOp::SetTime:
        case ControlOp::PinChange:
        case ControlOp::TmotorDirForward:
        case ControlOp::TmotorDirReverse:
        case ControlOp::DfuBegin:
        case ControlOp::DfuCommit:
        case ControlOp::DfuAbort:
        case ControlOp::DfuStatus:
            return true;
        default:
            return false;
    }
}

inline bool opRequiresAuth(uint8_t op) {
    switch (op) {
        case ControlOp::Auth:           // authenticating cannot require auth
        case ControlOp::CfgGet:         // read-only
        case ControlOp::BmsScanStatus:  // read-only: a plain status getter
        case ControlOp::DfuStatus:      // read-only: polled during a transfer
            return false;
        default:
            return true;
    }
}

inline bool opAllowedWhileArmed(uint8_t op) {
    switch (op) {
        case ControlOp::Auth:
        case ControlOp::CfgGet:
        case ControlOp::BmsScanStatus:
            return true;
        // BmsDetect is deliberately NOT here, despite reading like a query.
        // BluetoothBms::detectBmsTypeByMac() disables all three BMS drivers,
        // pauses advertising, and performs a BLOCKING BLEClient::connect() to
        // the given MAC. On an unresponsive address that can outlast the 10 s
        // task watchdog (WDT_TIMEOUT_S, panic=true), rebooting the controller
        // -- in flight, that cuts the motor. It also drops live battery
        // telemetry for the duration.
        case ControlOp::SessionReset:
            // A RAM-only flight-clock counter. It cannot reach the motor, and
            // it is the one write a pilot might plausibly want in flight.
            return true;
        case ControlOp::DfuStatus:
            // Read-only; polled during a transfer including while aircraft armed.
            return true;
        default:
            return false;
    }
}

// The single decision point. Order matters: armed is reported before auth so
// the app never prompts for a PIN to do something that is refused anyway.
inline ControlStatus gateRequest(uint8_t op, bool authenticated, bool armed) {
    if (!opIsKnown(op)) {
        return ControlStatus::ErrBadOp;
    }
    if (armed && !opAllowedWhileArmed(op)) {
        return ControlStatus::ErrState;
    }
    if (opRequiresAuth(op) && !authenticated) {
        return ControlStatus::ErrAuth;
    }
    return ControlStatus::Ok;
}

// ---------------------------------------------------------------------------
// Configuration groups
//
// Grouped rather than keyed, because fields like motorTempReductionStart and
// motorMaxTemp are only meaningful as a pair -- a per-key protocol would have
// to accept a transiently inconsistent combination.
//
// Same append rule as telemetry: CFG_SET applies only the prefix it received,
// overlaid on the current values, so an older app never zeroes a field it
// does not know about.
//
// Two encodings differ from the Settings accessors, which return types that
// cannot be memcpy'd:
//   - voltageDividerRatio is a float in Settings; here it is u16 hundredths.
//   - bmsMac / remoteMac are String in Settings; here they are 6 raw bytes.
//     All zero means unset, which is what "" means today.
// ---------------------------------------------------------------------------

#pragma pack(push, 1)
struct ConfigPower {
    uint16_t batteryCapacityMah;
    uint16_t batteryMinVoltageMv;
    uint16_t batteryMaxVoltageMv;
    uint8_t  powerControlEnabled;
    uint16_t voltageDividerRatioX100;
};

struct ConfigThermal {
    int32_t motorTempReductionStartMc;
    int32_t motorMaxTempMc;
    int32_t escTempReductionStartMc;
    int32_t escMaxTempMc;
    uint8_t motorTempSource;   // ignored on XAG; capability bit says so
};

struct ConfigBms {
    uint8_t bmsType;
    uint8_t bmsMac[6];
};

struct ConfigSystem {
    uint8_t buzzerVolume;
    uint8_t throttleSource;
    uint8_t remoteMac[6];
};
#pragma pack(pop)

inline bool decodeConfigGroup(const ControlRequest& req, ConfigGroup& out) {
    if (req.len < 1) {
        return false;
    }
    switch (req.payload[0]) {
        case (uint8_t) ConfigGroup::Power:
        case (uint8_t) ConfigGroup::Thermal:
        case (uint8_t) ConfigGroup::Bms:
        case (uint8_t) ConfigGroup::System:
            out = (ConfigGroup) req.payload[0];
            return true;
        default:
            return false;
    }
}

// ---------------------------------------------------------------------------
// Deferred request queue
//
// The CMD write callback runs on the Bluedroid task. Nothing there touches
// controller state: it only enqueues, and handle() drains the queue on the
// loop task. Same rule that keeps /api/session/reset off the web-server task,
// applied uniformly rather than per opcode.
// ---------------------------------------------------------------------------

#define CONTROL_QUEUED_PAYLOAD_MAX 32
#define CONTROL_QUEUE_CAPACITY      4

// "no central" for an authenticated-connection id. Real conn_ids are small
// indices, so 0xFFFF can never collide with one.
#define CONTROL_NO_CONN_ID 0xFFFF

struct QueuedRequest {
    uint8_t  op;
    uint8_t  seq;
    uint8_t  len;
    uint8_t  payload[CONTROL_QUEUED_PAYLOAD_MAX];
    // Which central sent this. Carried through the queue because the auth
    // check happens at dispatch, not at enqueue, and authentication belongs
    // to one connection rather than to the service. Not a wire field --
    // it comes from the GATT write event, not from the frame.
    uint16_t connId;
};

class ControlRequestQueue {
public:
    // Drop-newest on overflow: a flood must not evict a command the pilot
    // already issued and is waiting on.
    bool push(const ControlRequest& req, uint16_t connId) {
        if (count_ >= CONTROL_QUEUE_CAPACITY || req.len > CONTROL_QUEUED_PAYLOAD_MAX) {
            return false;
        }
        QueuedRequest& slot = items_[(head_ + count_) % CONTROL_QUEUE_CAPACITY];
        slot.op     = req.op;
        slot.seq    = req.seq;
        slot.len    = req.len;
        slot.connId = connId;
        if (req.len > 0 && req.payload != nullptr) {
            memcpy(slot.payload, req.payload, req.len);
        }
        count_++;
        return true;
    }

    bool pop(QueuedRequest& out) {
        if (count_ == 0) {
            return false;
        }
        out = items_[head_];
        head_ = (uint8_t) ((head_ + 1) % CONTROL_QUEUE_CAPACITY);
        count_--;
        return true;
    }

    uint8_t size() const { return count_; }

private:
    QueuedRequest items_[CONTROL_QUEUE_CAPACITY];
    uint8_t head_  = 0;
    uint8_t count_ = 0;
};

// ---------------------------------------------------------------------------
// Events (RSP with seq = CONTROL_EVENT_SEQ)
//
// The web page polls Sound's ring buffer and de-duplicates by seq. Over BLE
// that is unnecessary: the event is pushed at the moment the sound happens.
// The ring is still the source, so a high-water mark keeps each event from
// being resent on every tick.
// ---------------------------------------------------------------------------

#pragma pack(push, 1)
struct ControlBeepEvent {
    uint32_t seq;
    uint16_t frequency;
    uint16_t onMs;
    uint16_t offMs;
    uint8_t  reps;    // 0 = continuous
    uint8_t  layer;   // 0 = queued event, 1 = persistent state
    uint8_t  active;  // 1 = started, 0 = stopped
};
#pragma pack(pop)

// True when this ring entry has not been sent yet. seq 0 marks an empty slot.
// Updates the watermark, so each event goes out exactly once.
inline bool beepEventIsNew(uint32_t eventSeq, uint32_t& watermark) {
    if (eventSeq == 0 || eventSeq <= watermark) {
        return false;
    }
    watermark = eventSeq;
    return true;
}

// ---------------------------------------------------------------------------
// Firmware update
//
// Bulk data does NOT go through CMD: CONTROL_QUEUED_PAYLOAD_MAX is 32 bytes
// and the queue is four deep and drops the newest, so a 1.8 MB image would be
// ~57,000 round trips. It goes to BLE_CONTROL_DFU_UUID as
// [offset u32 LE][data...], written WITHOUT response -- which is what makes
// the transfer take a minute instead of ten, and which guarantees nothing.
// That is why the offset is absolute and present in every packet, and why the
// image carries a CRC32.
// ---------------------------------------------------------------------------

enum class DfuState : uint8_t {
    Idle      = 0,
    Receiving = 1,
    Verifying = 2,
    Ready     = 3,
    Error     = 4,
};

// An image can never exceed one OTA slot (min_spiffs.csv: app0 = 0x1E0000).
#define DFU_MAX_IMAGE_BYTES 0x1E0000

#pragma pack(push, 1)
struct DfuBeginRequest {
    uint32_t size;
    uint32_t crc32;
};

struct DfuBeginResponse {
    // Usable image bytes per data packet: the negotiated ATT MTU minus 3 for
    // ATT overhead, and the client subtracts 4 more for the offset header.
    uint16_t chunkSize;
};

struct DfuStatusResponse {
    uint8_t  state;      // DfuState
    uint32_t received;   // highest contiguous offset accepted
    uint16_t chunkSize;
};
#pragma pack(pop)

#endif // CONTROL_PROTOCOL_H
