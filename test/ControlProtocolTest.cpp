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

void test_telemetry_layout_is_pinned() {
    // Every offset is part of the contract with fly-app's Dart decoder.
    // A field inserted in the middle silently shifts everything after it,
    // which is the exact failure mode the $XCTOD sentence has.
    assert(sizeof(ControlTelemetry) == 56);
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

int main() {
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
    cout << "ControlProtocolTest: all passed" << endl;
    return 0;
}
