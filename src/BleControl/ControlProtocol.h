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
        BmsScanResult    = 0x2B,

        // Flight logs. 0x44-0x4F stay reserved.
        LogList      = 0x40,
        LogRead      = 0x41,
        LogDelete    = 0x42,
        LogDeleteAll = 0x43,

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
    ErrNotFound = 6, // a well-formed name that does not exist
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
    // Compiler build stamp. APP_VERSION is only meaningful on CI-tagged
    // images -- every local build reports "dev", so two images a week apart
    // carry the same version string and only the date tells them apart.
    char     buildDate[12];    // __DATE__ is 11 chars ("Sep 12 2026") + NUL
    char     buildTime[9];     // __TIME__ is  8 chars ("12:46:03") + NUL
    // Appended: Settings::getDefaultVoltageDividerRatio() x 100, so the app
    // can tell a calibrated ratio from the factory one and restore it.
    uint16_t defaultDividerRatioX100;
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

namespace BmsLinkState {
    enum : uint8_t {
        NotConfigured = 0,
        Idle          = 1,
        Connecting    = 2,
        Connected     = 3,
    };
}

// Maps BluetoothBms::getConnectionState() (which forwards JkBms::getStateName()
// for JK) onto the wire enum. JBD/Daly report only connected/connecting; JK
// adds idle and an "unknown" catch-all, which is treated as idle. The caller
// still forces NotConfigured when no BMS type/MAC is configured.
inline uint8_t bmsLinkStateFromName(const char* name) {
    if (name == nullptr || strcmp(name, "none") == 0) {
        return BmsLinkState::NotConfigured;
    }
    if (strcmp(name, "connected") == 0) {
        return BmsLinkState::Connected;
    }
    if (strcmp(name, "connecting") == 0) {
        return BmsLinkState::Connecting;
    }
    return BmsLinkState::Idle;
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
    // Appended: the BMS's own pack readings, covered by TelemValid::Bms like
    // the cell fields above (zero with the bit clear when unavailable).
    // u32 because packs above 65.5 V exist for the supported BMS types.
    uint32_t bmsPackMv;
    int32_t  bmsCurrentMa;   // signed: charging is a legitimate reading
    uint8_t  bmsSoc;         // %
    uint8_t  bmsCellCount;
    // Always meaningful, no validity bit: BmsLinkState below.
    uint8_t  bmsLinkState;
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
//  - Auth: reads cannot change anything, so they need no PIN; anything that
//    writes does.
//  - Armed: refuse by DEFAULT, allow by exception. With the phone in a
//    pocket, anything that can reach the motor is a hazard in flight, so the
//    default is inverted rather than enumerated.
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
        case ControlOp::BmsScanResult:
        case ControlOp::LogList:
        case ControlOp::LogRead:
        case ControlOp::LogDelete:
        case ControlOp::LogDeleteAll:
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
        case ControlOp::BmsScanResult:  // read-only: one stored scan result
        case ControlOp::LogList:        // read-only
        case ControlOp::LogRead:        // read-only; deletes need the PIN
        case ControlOp::DfuStatus:      // read-only: polled during a transfer
        case ControlOp::SetTime:
            // The worst abuse is a wrong timestamp in the log -- it cannot
            // reach the motor, any setting or the BMS. Still refused while
            // armed (see opAllowedWhileArmed).
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
        case ControlOp::BmsScanResult:
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

// Refused with ErrBusy while a firmware image is being received. The DFU
// flush and LittleFS share the flash and the loop task, and Update.write()
// already stalls a tick for ~150 ms per block erase; a delete-all or a burst
// of reads on top of that only lengthens ticks the watchdog is counting.
// Applied AFTER gateRequest(), so ErrState and ErrAuth keep their precedence.
inline bool opRefusedDuringDfu(uint8_t op) {
    switch (op) {
        case ControlOp::LogList:
        case ControlOp::LogRead:
        case ControlOp::LogDelete:
        case ControlOp::LogDeleteAll:
            return true;
        default:
            return false;
    }
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
// loop task. The rule is applied uniformly rather than per opcode.
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
// Sound keeps a ring buffer of recent beeps; a high-water mark keeps each
// event from being resent on every tick. The event is pushed at the moment
// the sound happens.
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

// ---------------------------------------------------------------------------
// Reply sizing
//
// Every reply must fit ONE notification at the NEGOTIATED MTU, not just
// CONTROL_MAX_PAYLOAD: the RSP header (4) plus payload must fit MTU - 3. iOS
// settles at 185, so a 240-byte reply would be cut by the stack and the app
// would reject it as truncated -- working on Android, timing out on iPhone.
// ---------------------------------------------------------------------------

inline size_t rspPayloadLimit(uint16_t mtu) {
    if (mtu <= 7) {
        return 0;
    }
    const size_t limit = (size_t) mtu - 7;
    return limit < CONTROL_MAX_PAYLOAD ? limit : CONTROL_MAX_PAYLOAD;
}

// ---------------------------------------------------------------------------
// Flight logs (0x40-0x43)
//
// Offset reads over CMD/RSP: each LOG_READ is idempotent, so a lost chunk is
// simply asked for again. No CRC -- the link layer acknowledges both
// directions here (unlike DFU's write-without-response), the echoed offset
// catches a misplaced chunk, and a CRC computed from the same flash the bytes
// are read from detects nothing the transport does not.
//
// Names travel WITHOUT the leading '/'. Validity is isValidLogName() in
// src/Logger/LogListing.h, shared with the Logger's retention.
// ---------------------------------------------------------------------------

#define LOG_MAX_NAME_LEN 24
#define LOG_MAX_CHUNK    232   // CONTROL_MAX_PAYLOAD - [offset u32][fileSize u32]

struct LogNameRef {
    const uint8_t* name;
    uint8_t        len;
};

struct LogReadRequest {
    uint32_t   offset;
    uint8_t    maxLen;
    LogNameRef name;
};

// [len u8][name...] -- the LOG_LIST cursor and the LOG_DELETE name.
inline bool decodeLogNameField(const uint8_t* p, size_t n, LogNameRef& out) {
    if (p == nullptr || n < 1 || (size_t) 1 + p[0] > n) {
        return false;
    }
    out.len  = p[0];
    out.name = p + 1;
    return true;
}

// [offset u32][maxLen u8][nameLen u8][name...]. maxLen 0 is rejected: its
// empty reply would be indistinguishable from end of file.
inline bool decodeLogReadRequest(const uint8_t* p, size_t n, LogReadRequest& out) {
    if (p == nullptr || n < 6) {
        return false;
    }
    memcpy(&out.offset, p, sizeof(out.offset));
    out.maxLen = p[4];
    if (out.maxLen == 0) {
        return false;
    }
    return decodeLogNameField(p + 5, n - 5, out.name);
}

// min(maxLen, LOG_MAX_CHUNK, MTU - 15, fileSize - offset). The MTU bound is
// RSP header 4 + [offset][fileSize] 8 + data <= MTU - 3. Zero at or past the
// end; the caller rejects offset > fileSize with ErrBadArg before this.
inline uint8_t logReadDataLength(uint8_t maxLen, uint16_t mtu,
                                 uint32_t fileSize, uint32_t offset) {
    if (offset >= fileSize || mtu <= 15) {
        return 0;
    }
    uint32_t n = maxLen < LOG_MAX_CHUNK ? maxLen : LOG_MAX_CHUNK;
    const uint32_t mtuCap = (uint32_t) mtu - 15;
    if (n > mtuCap) {
        n = mtuCap;
    }
    const uint32_t remaining = fileSize - offset;
    if (n > remaining) {
        n = remaining;
    }
    return (uint8_t) n;
}

// ---------------------------------------------------------------------------
// BMS_SCAN_RESULT (0x2B)
//
// [mac 6][rssi i8][type u8][nameLen u8][name...][svcLen u8][services...]
// The name is cut at BMS_SCAN_NAME_MAX first, then the services string so the
// whole reply fits `limit`. Returns the length, or 0 if not even the fixed
// fields fit. A UTF-8 name may be cut mid-character; the app decodes
// malformed bytes leniently.
// ---------------------------------------------------------------------------

#define BMS_SCAN_NAME_MAX 32

inline size_t encodeBmsScanResult(uint8_t* out, size_t limit, const uint8_t mac[6],
                                  int8_t rssi, uint8_t type,
                                  const char* name, size_t nameLen,
                                  const char* services, size_t svcLen) {
    const size_t fixed = 6 + 1 + 1 + 1 + 1;   // mac, rssi, type, nameLen, svcLen
    if (out == nullptr || mac == nullptr || limit < fixed) {
        return 0;
    }
    if (name == nullptr) {
        nameLen = 0;
    }
    if (services == nullptr) {
        svcLen = 0;
    }
    if (nameLen > BMS_SCAN_NAME_MAX) {
        nameLen = BMS_SCAN_NAME_MAX;
    }
    if (nameLen > limit - fixed) {
        nameLen = limit - fixed;
    }
    const size_t svcRoom = limit - fixed - nameLen;
    if (svcLen > svcRoom) {
        svcLen = svcRoom;
    }

    size_t o = 0;
    memcpy(out, mac, 6);
    o += 6;
    out[o++] = (uint8_t) rssi;
    out[o++] = type;
    out[o++] = (uint8_t) nameLen;
    if (nameLen > 0) {
        memcpy(out + o, name, nameLen);
        o += nameLen;
    }
    out[o++] = (uint8_t) svcLen;
    if (svcLen > 0) {
        memcpy(out + o, services, svcLen);
        o += svcLen;
    }
    return o;
}

#endif // CONTROL_PROTOCOL_H
