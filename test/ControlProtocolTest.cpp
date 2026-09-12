#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include "../src/BleControl/ControlProtocol.h"
using namespace std;

void test_protocol_version_is_declared() {
    // Duplicated byte-for-byte in fly-app. Bump both together, or the app
    // decodes a struct it does not have.
    assert(CONTROL_PROTOCOL_VERSION >= 1);
}

void test_decode_request_reads_header_and_payload() {
    const uint8_t frame[] = { 0x11, 0x07, 0x03, 0xAA, 0xBB, 0xCC };
    ControlRequest req;
    assert(decodeRequest(frame, sizeof(frame), req) == true);
    assert(req.op == 0x11);
    assert(req.seq == 0x07);
    assert(req.len == 3);
    assert(req.payload[0] == 0xAA);
    assert(req.payload[2] == 0xCC);
}

void test_decode_request_rejects_truncated_frames() {
    ControlRequest req;
    const uint8_t tooShort[] = { 0x11, 0x07 };            // no len byte
    assert(decodeRequest(tooShort, sizeof(tooShort), req) == false);

    const uint8_t lies[] = { 0x11, 0x07, 0x08, 0xAA };    // len says 8, has 1
    assert(decodeRequest(lies, sizeof(lies), req) == false);

    assert(decodeRequest(nullptr, 0, req) == false);
}

void test_decode_request_accepts_empty_payload() {
    const uint8_t frame[] = { 0x20, 0x01, 0x00 };
    ControlRequest req;
    assert(decodeRequest(frame, sizeof(frame), req) == true);
    assert(req.len == 0);
}

void test_encode_response_round_trips() {
    const uint8_t payload[] = { 0xDE, 0xAD };
    uint8_t out[CONTROL_MAX_FRAME];
    const size_t n = encodeResponse(out, sizeof(out), 0x10, 0x07,
                                    ControlStatus::Ok, payload, 2);
    assert(n == 6);
    assert(out[0] == 0x10);
    assert(out[1] == 0x07);
    assert(out[2] == (uint8_t) ControlStatus::Ok);
    assert(out[3] == 2);
    assert(out[4] == 0xDE && out[5] == 0xAD);
}

void test_encode_response_refuses_to_overflow() {
    uint8_t payload[CONTROL_MAX_PAYLOAD];
    memset(payload, 0, sizeof(payload));
    uint8_t tiny[4];
    // Does not fit: returns 0 and writes nothing rather than truncating into
    // a frame the app would decode as valid and short.
    assert(encodeResponse(tiny, sizeof(tiny), 0x10, 0x01,
                          ControlStatus::Ok, payload, CONTROL_MAX_PAYLOAD) == 0);
}

void test_encode_response_rejects_a_null_payload_with_nonzero_len() {
    uint8_t out[CONTROL_MAX_FRAME];
    // Promising bytes without providing them would ship uninitialised stack.
    assert(encodeResponse(out, sizeof(out), 0x10, 0x01,
                          ControlStatus::Ok, nullptr, 3) == 0);
}

void test_decode_request_rejects_an_oversized_len() {
    // Reachable in practice: a 247-byte MTU write can carry a len above
    // CONTROL_MAX_PAYLOAD with enough bytes behind it to clear the
    // truncation check, so this branch needs its own case.
    uint8_t frame[3 + 250];
    memset(frame, 0, sizeof(frame));
    frame[0] = 0x11;
    frame[1] = 0x01;
    frame[2] = 250;                       // > CONTROL_MAX_PAYLOAD (240)
    ControlRequest req;
    assert(decodeRequest(frame, sizeof(frame), req) == false);
}

void test_decode_request_accepts_len_exactly_at_the_maximum() {
    uint8_t frame[3 + CONTROL_MAX_PAYLOAD];
    memset(frame, 0xAB, sizeof(frame));
    frame[0] = 0x11;
    frame[1] = 0x01;
    frame[2] = CONTROL_MAX_PAYLOAD;
    ControlRequest req;
    assert(decodeRequest(frame, sizeof(frame), req) == true);
    assert(req.len == CONTROL_MAX_PAYLOAD);
    assert(req.payload[CONTROL_MAX_PAYLOAD - 1] == 0xAB);
}

void test_decode_request_accepts_n_exactly_three_plus_len() {
    const uint8_t frame[] = { 0x11, 0x01, 0x02, 0xAA, 0xBB };  // n == 5 == 3 + 2
    ControlRequest req;
    assert(decodeRequest(frame, sizeof(frame), req) == true);
    assert(req.len == 2);
    assert(req.payload[1] == 0xBB);
}

void test_encode_response_accepts_cap_exactly_equal_to_the_frame() {
    const uint8_t payload[] = { 0x01, 0x02 };
    uint8_t out[6];   // exactly 4 + 2, the boundary the comparison allows
    assert(encodeResponse(out, sizeof(out), 0x10, 0x01,
                          ControlStatus::Ok, payload, 2) == 6);
}

void test_events_use_seq_zero() {
    uint8_t out[CONTROL_MAX_FRAME];
    const size_t n = encodeResponse(out, sizeof(out), ControlOp::EvtBeep,
                                    CONTROL_EVENT_SEQ, ControlStatus::Ok,
                                    nullptr, 0);
    assert(n == 4);
    assert(out[1] == 0);
}

void test_info_layout_is_pinned() {
    // Hand-decoded in fly-app, same as ControlTelemetry, so it gets the same
    // protection.
    assert(sizeof(ControlInfo) == 49);
    assert(offsetof(ControlInfo, protocolVersion) ==  0);
    assert(offsetof(ControlInfo, controllerType)  ==  1);
    assert(offsetof(ControlInfo, capabilities)    ==  2);
    assert(offsetof(ControlInfo, appVersion)      ==  4);
    assert(offsetof(ControlInfo, buildDate)       == 28);
    assert(offsetof(ControlInfo, buildTime)       == 40);

    // The compiler's own stamps must fit with room for the NUL. Pinned here
    // rather than trusted: a shorter field would silently truncate the year.
    assert(sizeof(__DATE__) <= sizeof(((ControlInfo*) nullptr)->buildDate));
    assert(sizeof(__TIME__) <= sizeof(((ControlInfo*) nullptr)->buildTime));

    assert(Capability::CanTelemetry       == 0x0001);
    assert(Capability::VoltageSensor      == 0x0002);
    assert(Capability::MotorTempSourceSel == 0x0004);
    assert(Capability::RemoteLink         == 0x0008);
}

void test_telemetry_layout_is_pinned() {
    // Every offset is part of the contract with fly-app's Dart decoder.
    // A field inserted in the middle silently shifts everything after it,
    // which is the exact failure mode the $XCTOD sentence has.
    assert(sizeof(ControlTelemetry) == 58);
    assert(offsetof(ControlTelemetry, ver)            ==  0);
    assert(offsetof(ControlTelemetry, flags)          ==  1);
    assert(offsetof(ControlTelemetry, validity)       ==  2);
    assert(offsetof(ControlTelemetry, disarmReason)   ==  4);
    assert(offsetof(ControlTelemetry, signalStates)   ==  5);
    assert(offsetof(ControlTelemetry, motorTempSrc)   ==  6);
    assert(offsetof(ControlTelemetry, socCc)          ==  7);
    assert(offsetof(ControlTelemetry, socVolt)        ==  8);
    assert(offsetof(ControlTelemetry, throttlePct)    ==  9);
    assert(offsetof(ControlTelemetry, powerPct)       == 10);
    assert(offsetof(ControlTelemetry, powerScale)     == 11);
    assert(offsetof(ControlTelemetry, armCharge)      == 12);
    assert(offsetof(ControlTelemetry, limitCauses)    == 13);
    assert(offsetof(ControlTelemetry, batteryMv)      == 14);
    assert(offsetof(ControlTelemetry, throttleRaw)    == 16);
    assert(offsetof(ControlTelemetry, powerKwX10)     == 18);
    assert(offsetof(ControlTelemetry, escCurrentMa)   == 20);
    assert(offsetof(ControlTelemetry, rpm)            == 24);
    assert(offsetof(ControlTelemetry, motorTempMc)    == 28);
    assert(offsetof(ControlTelemetry, escTempMc)      == 32);
    assert(offsetof(ControlTelemetry, sessionSec)     == 36);
    assert(offsetof(ControlTelemetry, hourMeterSec)   == 40);
    assert(offsetof(ControlTelemetry, bmsCellMinMv)   == 44);
    assert(offsetof(ControlTelemetry, bmsCellMaxMv)   == 46);
    assert(offsetof(ControlTelemetry, bmsCellDeltaMv) == 48);
    assert(offsetof(ControlTelemetry, bmsTempMaxC)    == 50);
    assert(offsetof(ControlTelemetry, uptimeSec)      == 52);
    assert(offsetof(ControlTelemetry, stateFreqHz)    == 56);
}

void test_signal_states_pack_three_signals_into_one_byte() {
    // SignalState is 0..3, so each signal is exactly two bits.
    const uint8_t packed = packSignalStates(3, 1, 2); // Valid, Stale, Invalid
    assert(unpackMotorTempState(packed) == 3);
    assert(unpackEscTempState(packed)   == 1);
    assert(unpackBatteryVState(packed)  == 2);
}

void test_signal_states_round_trip_every_combination() {
    for (uint8_t m = 0; m < 4; m++) {
        for (uint8_t e = 0; e < 4; e++) {
            for (uint8_t b = 0; b < 4; b++) {
                const uint8_t packed = packSignalStates(m, e, b);
                assert(unpackMotorTempState(packed) == m);
                assert(unpackEscTempState(packed)   == e);
                assert(unpackBatteryVState(packed)  == b);
            }
        }
    }
}

void test_telemetry_bit_values_are_pinned() {
    // The Dart decoder hardcodes these. A reordered enum breaks the wire
    // contract exactly as a shifted field offset would, and offsetof cannot
    // see it.
    assert(TelemFlag::Armed               == 0x01);
    assert(TelemFlag::Engaged             == 0x02);
    assert(TelemFlag::HasTelemetry        == 0x04);
    assert(TelemFlag::PowerControlEnabled == 0x08);
    assert(TelemFlag::BmsConnected        == 0x10);
    assert(TelemFlag::BmsConfigured       == 0x20);

    assert(TelemValid::Current  == 0x0001);
    assert(TelemValid::Rpm      == 0x0002);
    assert(TelemValid::PowerKw  == 0x0004);
    assert(TelemValid::Bms      == 0x0008);
    assert(TelemValid::BmsCells == 0x0010);
}

void test_telemetry_serialises_little_endian_with_signed_fields() {
    // What actually goes over the air. Catches an endianness or signedness
    // mistake that the offset assertions cannot. Both the ESP32-C3 and every
    // host this test runs on are little-endian, so the byte order is a fair
    // thing to pin here.
    ControlTelemetry t;
    memset(&t, 0, sizeof(t));
    t.batteryMv    = 0x1234;
    t.escCurrentMa = -1000;      // regen: this field must stay signed
    t.rpm          = 0x89ABCDEF;
    t.bmsTempMaxC  = -5;

    const uint8_t* raw = (const uint8_t*) &t;
    assert(raw[14] == 0x34 && raw[15] == 0x12);

    int32_t current = 0;
    memcpy(&current, raw + 20, sizeof(current));
    assert(current == -1000);

    uint32_t rpm = 0;
    memcpy(&rpm, raw + 24, sizeof(rpm));
    assert(rpm == 0x89ABCDEFu);

    int16_t temp = 0;
    memcpy(&temp, raw + 50, sizeof(temp));
    assert(temp == -5);
}

void test_append_rule_long_source_into_short_reader() {
    // Newer firmware, older reader: the reader keeps the prefix it knows and
    // drops the tail it does not.
    uint8_t source[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    uint8_t reader[4] = { 0, 0, 0, 0 };
    assert(copyKnownPrefix(reader, sizeof(reader), source, sizeof(source)) == 4);
    assert(reader[0] == 1 && reader[3] == 4);
}

void test_append_rule_short_source_preserves_readers_tail() {
    // Older firmware, newer reader: the fields the sender does not know keep
    // whatever the reader already had -- for CFG_SET that is the current
    // setting, which must NOT be zeroed by a short write.
    uint8_t source[2] = { 9, 9 };
    uint8_t reader[5] = { 1, 2, 3, 4, 5 };
    assert(copyKnownPrefix(reader, sizeof(reader), source, sizeof(source)) == 2);
    assert(reader[0] == 9 && reader[1] == 9);
    assert(reader[2] == 3 && reader[3] == 4 && reader[4] == 5);
}

void test_append_rule_handles_empty_and_equal_sizes() {
    uint8_t reader[3] = { 1, 2, 3 };
    assert(copyKnownPrefix(reader, sizeof(reader), nullptr, 0) == 0);
    assert(reader[0] == 1);

    uint8_t source[3] = { 7, 8, 9 };
    assert(copyKnownPrefix(reader, sizeof(reader), source, sizeof(source)) == 3);
    assert(reader[2] == 9);
}

void test_gate_rejects_unknown_opcodes() {
    // ErrBadOp is what lets a newer app probe an older firmware and degrade.
    assert(gateRequest(0x7F, true, false) == ControlStatus::ErrBadOp);
    assert(gateRequest(0xFF, true, false) == ControlStatus::ErrBadOp);
}

void test_gate_allows_reads_without_auth() {
    assert(gateRequest(ControlOp::Auth,          false, false) == ControlStatus::Ok);
    assert(gateRequest(ControlOp::CfgGet,        false, false) == ControlStatus::Ok);
    assert(gateRequest(ControlOp::BmsScanStatus, false, false) == ControlStatus::Ok);
}

void test_gate_requires_auth_for_writes() {
    assert(gateRequest(ControlOp::CfgSet,       false, false) == ControlStatus::ErrAuth);
    assert(gateRequest(ControlOp::SessionReset, false, false) == ControlStatus::ErrAuth);
    assert(gateRequest(ControlOp::RemotePair,   false, false) == ControlStatus::ErrAuth);
    // BmsDetect reads like a query but tears down the BMS link and blocks on
    // a BLE connect -- it is a write in every way that matters.
    assert(gateRequest(ControlOp::BmsDetect,    false, false) == ControlStatus::ErrAuth);
    assert(gateRequest(ControlOp::BmsDetect,    true,  false) == ControlStatus::Ok);
    // ...and allows them once authenticated.
    assert(gateRequest(ControlOp::CfgSet,       true,  false) == ControlStatus::Ok);
    assert(gateRequest(ControlOp::SessionReset, true,  false) == ControlStatus::Ok);
}

void test_gate_blocks_by_default_while_armed() {
    assert(gateRequest(ControlOp::CfgSet,           true, true) == ControlStatus::ErrState);
    assert(gateRequest(ControlOp::BuzzerPreview,    true, true) == ControlStatus::ErrState);
    assert(gateRequest(ControlOp::BmsScanStart,     true, true) == ControlStatus::ErrState);
    assert(gateRequest(ControlOp::BmsDetect,        true, true) == ControlStatus::ErrState);
    assert(gateRequest(ControlOp::RemotePair,       true, true) == ControlStatus::ErrState);
    assert(gateRequest(ControlOp::RemoteForget,     true, true) == ControlStatus::ErrState);
    assert(gateRequest(ControlOp::SetTime,          true, true) == ControlStatus::ErrState);
    assert(gateRequest(ControlOp::PinChange,        true, true) == ControlStatus::ErrState);
    assert(gateRequest(ControlOp::TmotorDirForward, true, true) == ControlStatus::ErrState);
    assert(gateRequest(ControlOp::TmotorDirReverse, true, true) == ControlStatus::ErrState);
}

void test_gate_armed_exceptions() {
    // Read-only, or cannot reach the motor.
    assert(gateRequest(ControlOp::Auth,          true,  true) == ControlStatus::Ok);
    assert(gateRequest(ControlOp::CfgGet,        false, true) == ControlStatus::Ok);
    assert(gateRequest(ControlOp::BmsScanStatus, false, true) == ControlStatus::Ok);
    // SessionReset is a RAM-only flight clock; a pilot may want it in flight.
    assert(gateRequest(ControlOp::SessionReset,  true,  true) == ControlStatus::Ok);
}

void test_gate_reports_armed_before_auth() {
    // A blocked-while-armed op is refused with ErrState even when the caller
    // has not authenticated, so the app does not prompt for a PIN to perform
    // something that would be refused anyway.
    assert(gateRequest(ControlOp::CfgSet, false, true) == ControlStatus::ErrState);
}

void test_config_struct_sizes_are_pinned() {
    assert(sizeof(ConfigPower)   == 9);
    assert(sizeof(ConfigThermal) == 17);
    assert(sizeof(ConfigBms)     == 7);
    assert(sizeof(ConfigSystem)  == 8);
}

void test_config_set_partial_prefix_keeps_unknown_tail() {
    // An older app knows only the first three fields of ConfigPower. The
    // fields it does not send must keep their current values, not be zeroed.
    ConfigPower current = { 20000, 48000, 58800, 1, 1100 };
    const uint8_t shortWrite[] = { 0x10, 0x27, 0xC0, 0xBB, 0x88, 0xD3 }; // 10000, 48064, 54200
    copyKnownPrefix(&current, sizeof(current), shortWrite, sizeof(shortWrite));

    assert(current.batteryCapacityMah == 10000);
    assert(current.powerControlEnabled == 1);        // untouched
    assert(current.voltageDividerRatioX100 == 1100); // untouched
}

void test_config_set_ignores_a_longer_struct_than_we_know() {
    ConfigBms current = { 0, { 0, 0, 0, 0, 0, 0 } };
    uint8_t longWrite[16];
    memset(longWrite, 0xEE, sizeof(longWrite));
    longWrite[0] = 3;
    const size_t copied = copyKnownPrefix(&current, sizeof(current),
                                          longWrite, sizeof(longWrite));
    assert(copied == sizeof(ConfigBms));
    assert(current.bmsType == 3);
}

void test_config_group_is_decoded_from_the_payload() {
    const uint8_t frame[] = { ControlOp::CfgGet, 0x01, 0x01,
                              (uint8_t) ConfigGroup::Thermal };
    ControlRequest req;
    assert(decodeRequest(frame, sizeof(frame), req) == true);
    ConfigGroup group;
    assert(decodeConfigGroup(req, group) == true);
    assert(group == ConfigGroup::Thermal);
}

void test_config_group_rejects_missing_or_unknown_group() {
    ControlRequest req;

    const uint8_t noGroup[] = { ControlOp::CfgGet, 0x01, 0x00 };
    assert(decodeRequest(noGroup, sizeof(noGroup), req) == true);
    ConfigGroup group;
    assert(decodeConfigGroup(req, group) == false);

    const uint8_t badGroup[] = { ControlOp::CfgGet, 0x01, 0x01, 0x09 };
    assert(decodeRequest(badGroup, sizeof(badGroup), req) == true);
    assert(decodeConfigGroup(req, group) == false);
}

void test_request_queue_fifo_order() {
    ControlRequestQueue q;
    const uint8_t a[] = { 0xAA };
    const uint8_t b[] = { 0xBB };
    ControlRequest r1 = { ControlOp::CfgGet, 1, 1, a };
    ControlRequest r2 = { ControlOp::CfgSet, 2, 1, b };
    assert(q.push(r1, 7) == true);
    assert(q.push(r2, 9) == true);
    assert(q.size() == 2);

    QueuedRequest out;
    assert(q.pop(out) == true);
    assert(out.op == ControlOp::CfgGet && out.seq == 1 && out.payload[0] == 0xAA);
    assert(out.connId == 7);
    assert(q.pop(out) == true);
    assert(out.op == ControlOp::CfgSet && out.seq == 2 && out.payload[0] == 0xBB);
    assert(out.connId == 9);
    assert(q.pop(out) == false);
    assert(q.size() == 0);
}

void test_request_queue_drops_the_newest_when_full() {
    // Drop-newest, not drop-oldest: a flood must not evict a command the
    // pilot already issued and is waiting on.
    ControlRequestQueue q;
    const uint8_t p[] = { 0x01 };
    for (uint8_t i = 0; i < CONTROL_QUEUE_CAPACITY; i++) {
        ControlRequest r = { ControlOp::CfgGet, (uint8_t) (i + 1), 1, p };
        assert(q.push(r, 0) == true);
    }
    ControlRequest overflow = { ControlOp::CfgSet, 99, 1, p };
    assert(q.push(overflow, 0) == false);

    QueuedRequest out;
    assert(q.pop(out) == true);
    assert(out.seq == 1);  // the oldest survived
}

void test_request_queue_keeps_connections_distinct() {
    // Authentication belongs to one connection, and the auth check happens at
    // dispatch rather than at enqueue -- so the connection identity has to
    // survive the queue or one central's session would answer for another's.
    ControlRequestQueue q;
    const uint8_t p[] = { 0x01 };
    ControlRequest a = { ControlOp::CfgSet, 1, 1, p };
    ControlRequest b = { ControlOp::CfgSet, 2, 1, p };
    assert(q.push(a, 3) == true);
    assert(q.push(b, CONTROL_NO_CONN_ID) == true);

    QueuedRequest out;
    assert(q.pop(out) == true && out.connId == 3);
    assert(q.pop(out) == true && out.connId == CONTROL_NO_CONN_ID);
}

void test_request_queue_refuses_oversized_payloads() {
    ControlRequestQueue q;
    uint8_t big[CONTROL_QUEUED_PAYLOAD_MAX + 1];
    memset(big, 0, sizeof(big));
    ControlRequest r = { ControlOp::CfgSet, 1, CONTROL_QUEUED_PAYLOAD_MAX + 1, big };
    assert(q.push(r, 0) == false);
    assert(q.size() == 0);
}

void test_beep_event_payload_is_fixed_size() {
    // 4 + 2 + 2 + 2 + 1 + 1 + 1
    assert(sizeof(ControlBeepEvent) == 13);
}

void test_beep_watermark_emits_each_event_once() {
    // Sound's ring buffer is a snapshot, re-read every tick. Only events
    // newer than the last one sent may go out, or the app replays old beeps
    // at 1 Hz forever.
    uint32_t watermark = 0;
    assert(beepEventIsNew(5, watermark) == true);
    assert(watermark == 5);
    assert(beepEventIsNew(5, watermark) == false);   // same event, second tick
    assert(beepEventIsNew(3, watermark) == false);   // older slot in the ring
    assert(beepEventIsNew(6, watermark) == true);
    assert(watermark == 6);
}

void test_beep_watermark_ignores_empty_slots() {
    // seq 0 marks an empty ring slot (see Sound::BeepEvent).
    uint32_t watermark = 0;
    assert(beepEventIsNew(0, watermark) == false);
    assert(watermark == 0);
}

void test_dfu_layout_is_pinned() {
    assert(sizeof(DfuBeginRequest)  == 8);
    assert(sizeof(DfuBeginResponse) == 2);
    assert(sizeof(DfuStatusResponse) == 7);
    assert(offsetof(DfuBeginRequest,   size)      == 0);
    assert(offsetof(DfuBeginRequest,   crc32)     == 4);
    assert(offsetof(DfuStatusResponse, state)     == 0);
    assert(offsetof(DfuStatusResponse, received)  == 1);
    assert(offsetof(DfuStatusResponse, chunkSize) == 5);
}

void test_dfu_state_values_are_pinned() {
    // Decoded by hand in fly-app.
    assert((uint8_t) DfuState::Idle      == 0);
    assert((uint8_t) DfuState::Receiving == 1);
    assert((uint8_t) DfuState::Verifying == 2);
    assert((uint8_t) DfuState::Ready     == 3);
    assert((uint8_t) DfuState::Error     == 4);
}

void test_dfu_opcode_values_are_pinned() {
    assert(ControlOp::DfuBegin  == 0x50);
    assert(ControlOp::DfuCommit == 0x51);
    assert(ControlOp::DfuAbort  == 0x52);
    assert(ControlOp::DfuStatus == 0x53);
}

void test_dfu_status_is_open_and_allowed_in_flight() {
    // The app polls it once a second during a transfer and must not be asked
    // for a PIN, nor refused because the aircraft happens to be armed.
    assert(gateRequest(ControlOp::DfuStatus, false, false) == ControlStatus::Ok);
    assert(gateRequest(ControlOp::DfuStatus, false, true)  == ControlStatus::Ok);
}

void test_dfu_writes_need_auth_and_are_refused_in_flight() {
    assert(gateRequest(ControlOp::DfuBegin,  false, false) == ControlStatus::ErrAuth);
    assert(gateRequest(ControlOp::DfuCommit, false, false) == ControlStatus::ErrAuth);
    assert(gateRequest(ControlOp::DfuAbort,  false, false) == ControlStatus::ErrAuth);

    assert(gateRequest(ControlOp::DfuBegin,  true, false) == ControlStatus::Ok);
    assert(gateRequest(ControlOp::DfuCommit, true, false) == ControlStatus::Ok);
    assert(gateRequest(ControlOp::DfuAbort,  true, false) == ControlStatus::Ok);

    // Flashing firmware mid-flight is the clearest case the armed gate exists
    // for. ErrState comes before ErrAuth, so an unauthenticated client is not
    // told to find a PIN for something refused either way.
    assert(gateRequest(ControlOp::DfuBegin,  true,  true) == ControlStatus::ErrState);
    assert(gateRequest(ControlOp::DfuCommit, true,  true) == ControlStatus::ErrState);
    assert(gateRequest(ControlOp::DfuAbort,  true,  true) == ControlStatus::ErrState);
    assert(gateRequest(ControlOp::DfuBegin,  false, true) == ControlStatus::ErrState);
}

void test_the_rest_of_the_dfu_range_stays_unknown() {
    // Only the four defined opcodes exist; the reserved range is not a
    // blanket permit.
    assert(gateRequest(0x54, true, false) == ControlStatus::ErrBadOp);
    assert(gateRequest(0x5F, true, false) == ControlStatus::ErrBadOp);
}

int main() {
    test_info_layout_is_pinned();
    test_telemetry_layout_is_pinned();
    test_signal_states_pack_three_signals_into_one_byte();
    test_signal_states_round_trip_every_combination();
    test_telemetry_bit_values_are_pinned();
    test_telemetry_serialises_little_endian_with_signed_fields();
    test_append_rule_long_source_into_short_reader();
    test_append_rule_short_source_preserves_readers_tail();
    test_append_rule_handles_empty_and_equal_sizes();
    test_protocol_version_is_declared();
    test_decode_request_reads_header_and_payload();
    test_decode_request_rejects_truncated_frames();
    test_decode_request_accepts_empty_payload();
    test_encode_response_round_trips();
    test_encode_response_refuses_to_overflow();
    test_encode_response_rejects_a_null_payload_with_nonzero_len();
    test_decode_request_rejects_an_oversized_len();
    test_decode_request_accepts_len_exactly_at_the_maximum();
    test_decode_request_accepts_n_exactly_three_plus_len();
    test_encode_response_accepts_cap_exactly_equal_to_the_frame();
    test_events_use_seq_zero();
    test_gate_rejects_unknown_opcodes();
    test_gate_allows_reads_without_auth();
    test_gate_requires_auth_for_writes();
    test_gate_blocks_by_default_while_armed();
    test_gate_armed_exceptions();
    test_gate_reports_armed_before_auth();
    test_config_struct_sizes_are_pinned();
    test_config_set_partial_prefix_keeps_unknown_tail();
    test_config_set_ignores_a_longer_struct_than_we_know();
    test_config_group_is_decoded_from_the_payload();
    test_config_group_rejects_missing_or_unknown_group();
    test_request_queue_fifo_order();
    test_request_queue_drops_the_newest_when_full();
    test_request_queue_keeps_connections_distinct();
    test_request_queue_refuses_oversized_payloads();
    test_beep_event_payload_is_fixed_size();
    test_beep_watermark_emits_each_event_once();
    test_beep_watermark_ignores_empty_slots();
    test_dfu_layout_is_pinned();
    test_dfu_state_values_are_pinned();
    test_dfu_opcode_values_are_pinned();
    test_dfu_status_is_open_and_allowed_in_flight();
    test_dfu_writes_need_auth_and_are_refused_in_flight();
    test_the_rest_of_the_dfu_range_stays_unknown();
    cout << "ControlProtocolTest: all passed" << endl;
    return 0;
}
