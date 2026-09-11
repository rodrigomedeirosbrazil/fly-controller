#include <cassert>
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

int main() {
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
