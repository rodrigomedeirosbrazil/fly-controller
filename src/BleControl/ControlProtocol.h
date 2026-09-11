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

#endif // CONTROL_PROTOCOL_H
