#include <cassert>
#include <iostream>
#include <cstring>
#include "../src/BleControl/DfuSession.h"

using std::cout;
using std::endl;

// Simple fold-sum CRC for testing (order-sensitive, deterministic)
uint32_t testCrc(uint32_t crc, const uint8_t* data, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

void test_begin_rejects_zero_size() {
    DfuSession session(testCrc);
    assert(session.state() == DfuState::Idle);
    bool result = session.begin(0, 0x12345678);
    assert(result == false);
    assert(session.state() == DfuState::Error);
    cout << "PASS: begin rejects zero size" << endl;
}

void test_begin_rejects_oversized() {
    DfuSession session(testCrc);
    bool result = session.begin(DFU_MAX_IMAGE_BYTES + 1, 0x12345678);
    assert(result == false);
    assert(session.state() == DfuState::Error);
    cout << "PASS: begin rejects DFU_MAX_IMAGE_BYTES + 1" << endl;
}

void test_begin_accepts_one_byte() {
    DfuSession session(testCrc);
    bool result = session.begin(1, 0x12345678);
    assert(result == true);
    assert(session.state() == DfuState::Receiving);
    assert(session.size() == 1);
    assert(session.received() == 0);
    cout << "PASS: begin accepts 1 byte" << endl;
}

void test_begin_accepts_max_size() {
    DfuSession session(testCrc);
    bool result = session.begin(DFU_MAX_IMAGE_BYTES, 0x12345678);
    assert(result == true);
    assert(session.state() == DfuState::Receiving);
    assert(session.size() == DFU_MAX_IMAGE_BYTES);
    cout << "PASS: begin accepts DFU_MAX_IMAGE_BYTES" << endl;
}

void test_first_packet_at_offset_zero() {
    DfuSession session(testCrc);
    session.begin(10, 0xDEADBEEF);

    uint8_t data[5] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
    bool result = session.acceptOffset(0, 5);
    assert(result == true);
    assert(session.received() == 5);
    cout << "PASS: first packet at offset 0 accepted" << endl;
}

void test_out_of_order_packet_rejected() {
    DfuSession session(testCrc);
    session.begin(100, 0xDEADBEEF);

    uint8_t data[5] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
    // Accept first packet
    session.acceptOffset(0, 5);
    uint32_t receivedBefore = session.received();

    // Try to skip ahead (offset > received)
    bool result = session.acceptOffset(20, 5);
    assert(result == false);
    assert(session.received() == receivedBefore);
    cout << "PASS: out-of-order packet rejected" << endl;
}

void test_resend_packet_rejected() {
    DfuSession session(testCrc);
    session.begin(100, 0xDEADBEEF);

    uint8_t data[5] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
    // Accept first packet
    session.acceptOffset(0, 5);
    uint32_t receivedBefore = session.received();

    // Try to resend earlier data (offset < received)
    bool result = session.acceptOffset(2, 5);
    assert(result == false);
    assert(session.received() == receivedBefore);
    cout << "PASS: resend packet rejected" << endl;
}

void test_packet_overshoot_rejected() {
    DfuSession session(testCrc);
    session.begin(10, 0xDEADBEEF);

    // Accept first 8 bytes
    session.acceptOffset(0, 8);
    uint32_t receivedBefore = session.received();

    // Try to add 5 more when only 2 bytes remain
    bool result = session.acceptOffset(8, 5);
    assert(result == false);
    assert(session.received() == receivedBefore);
    cout << "PASS: packet overshoot rejected" << endl;
}

void test_exact_fit_final_packet() {
    DfuSession session(testCrc);
    session.begin(10, 0xDEADBEEF);

    // First 5 bytes
    session.acceptOffset(0, 5);
    // Last 5 bytes
    bool result = session.acceptOffset(5, 5);
    assert(result == true);
    assert(session.received() == 10);
    assert(session.isComplete() == true);
    cout << "PASS: exact-fit final packet completes" << endl;
}

void test_commit_allowed_with_matching_crc() {
    DfuSession session(testCrc);
    session.begin(5, 0x12345678);

    uint8_t data[5] = {0x01, 0x02, 0x03, 0x04, 0x05};
    session.acceptOffset(0, 5);
    session.accumulate(data, 5);

    uint32_t expectedCrc = testCrc(0, data, 5);

    // Create new session with the correct CRC
    DfuSession session2(testCrc);
    session2.begin(5, expectedCrc);
    session2.acceptOffset(0, 5);
    session2.accumulate(data, 5);

    assert(session2.isComplete() == true);
    assert(session2.commitAllowed() == true);
    cout << "PASS: commitAllowed() true with matching CRC" << endl;
}

void test_commit_not_allowed_with_wrong_crc() {
    DfuSession session(testCrc);
    session.begin(5, 0xDEADBEEF);  // Intentionally wrong CRC

    uint8_t data[5] = {0x01, 0x02, 0x03, 0x04, 0x05};
    session.acceptOffset(0, 5);
    session.accumulate(data, 5);

    assert(session.isComplete() == true);
    assert(session.commitAllowed() == false);
    cout << "PASS: commitAllowed() false with mismatched CRC" << endl;
}

void test_commit_not_allowed_when_incomplete() {
    DfuSession session(testCrc);
    uint8_t data[5] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint32_t expectedCrc = testCrc(0, data, 5);

    session.begin(10, expectedCrc);  // Size is 10 bytes
    session.acceptOffset(0, 5);       // But only 5 received
    session.accumulate(data, 5);

    assert(session.isComplete() == false);
    assert(session.commitAllowed() == false);
    cout << "PASS: commitAllowed() false when incomplete" << endl;
}

void test_abort_before_begin() {
    DfuSession session(testCrc);
    session.abort();  // Should not crash
    assert(session.state() == DfuState::Idle);
    assert(session.received() == 0);
    cout << "PASS: abort() before begin() is safe" << endl;
}

void test_abort_mid_transfer() {
    DfuSession session(testCrc);
    session.begin(100, 0xDEADBEEF);

    uint8_t data[10] = {0};
    session.acceptOffset(0, 10);
    session.accumulate(data, 10);

    assert(session.received() == 10);
    session.abort();

    assert(session.state() == DfuState::Idle);
    assert(session.received() == 0);
    assert(session.size() == 0);
    cout << "PASS: abort() mid-transfer resets state" << endl;
}

void test_second_begin_after_abort() {
    DfuSession session(testCrc);
    session.begin(100, 0xDEADBEEF);

    uint8_t data[10] = {0};
    session.acceptOffset(0, 10);
    session.abort();

    // Start a new transfer
    bool result = session.begin(50, 0xCAFEBABE);
    assert(result == true);
    assert(session.state() == DfuState::Receiving);
    assert(session.size() == 50);
    assert(session.received() == 0);

    // Verify we can accept fresh packets
    bool accept = session.acceptOffset(0, 10);
    assert(accept == true);
    assert(session.received() == 10);
    cout << "PASS: second begin() after abort() starts clean" << endl;
}

void test_accumulate_updates_crc() {
    DfuSession session(testCrc);
    session.begin(100, 0x12345678);

    uint8_t data1[3] = {0xAA, 0xBB, 0xCC};
    uint8_t data2[2] = {0xDD, 0xEE};

    session.acceptOffset(0, 3);
    session.accumulate(data1, 3);
    uint32_t crc1 = session.runningCrc();

    session.acceptOffset(3, 2);
    session.accumulate(data2, 2);
    uint32_t crc2 = session.runningCrc();

    // CRC should have changed
    assert(crc1 != crc2);

    // Verify incremental vs all-at-once match
    DfuSession session2(testCrc);
    session2.begin(100, 0x12345678);
    session2.acceptOffset(0, 5);
    uint8_t allData[5] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
    session2.accumulate(allData, 5);

    assert(session2.runningCrc() == crc2);
    cout << "PASS: accumulate() updates CRC correctly" << endl;
}

void test_a_huge_length_cannot_wrap_the_overshoot_check() {
    // received_ + len is uint32 and wraps: at received_ = 100 a len of
    // 0xFFFFFFFF sums to 99, which would slip past a naive bounds check and
    // then move received_ backwards.
    DfuSession s(testCrc);
    assert(s.begin(1000, 0) == true);
    assert(s.acceptOffset(0, 100) == true);
    assert(s.received() == 100);

    assert(s.acceptOffset(100, 0xFFFFFFFFu) == false);
    assert(s.received() == 100);   // unmoved
    cout << "PASS: a wrapping length cannot pass the overshoot check" << endl;
}

int main() {
    test_a_huge_length_cannot_wrap_the_overshoot_check();
    test_begin_rejects_zero_size();
    test_begin_rejects_oversized();
    test_begin_accepts_one_byte();
    test_begin_accepts_max_size();
    test_first_packet_at_offset_zero();
    test_out_of_order_packet_rejected();
    test_resend_packet_rejected();
    test_packet_overshoot_rejected();
    test_exact_fit_final_packet();
    test_commit_allowed_with_matching_crc();
    test_commit_not_allowed_with_wrong_crc();
    test_commit_not_allowed_when_incomplete();
    test_abort_before_begin();
    test_abort_mid_transfer();
    test_second_begin_after_abort();
    test_accumulate_updates_crc();

    cout << endl << "All tests passed!" << endl;
    return 0;
}
