// test/EscStatusRpmTest.cpp
// Compile: c++ -std=c++17 -Wall test/EscStatusRpmTest.cpp -o "${TMPDIR:-/tmp}/esc_status_rpm_test" && "${TMPDIR:-/tmp}/esc_status_rpm_test"
#include <cassert>
#include <iostream>
#include "../src/Tmotor/EscStatusRpm.h"
using namespace std;

void test_short_frame_keeps_previous_value() {
    uint8_t p[14] = {0};
    uint16_t rpm = 1234;
    assert(!parseEscStatusRpm(p, 14, &rpm));
    assert(rpm == 1234);
}

void test_positive_rpm() {
    uint8_t p[15] = {0};
    p[12] = 0x58; p[13] = 0x02; p[14] = 0x00;
    uint16_t rpm = 0;
    assert(parseEscStatusRpm(p, 15, &rpm));
    assert(rpm == 600);
}

void test_negative_rpm_is_absolute() {
    // -600 as int18: 0x3FFFF - 599 = 0x3FDA8 -> bytes A8 FD 03
    uint8_t p[15] = {0};
    p[12] = 0xA8; p[13] = 0xFD; p[14] = 0x03;
    uint16_t rpm = 0;
    assert(parseEscStatusRpm(p, 15, &rpm));
    assert(rpm == 600);
}

void test_real_zero_is_true() {
    uint8_t p[15] = {0};
    uint16_t rpm = 99;
    assert(parseEscStatusRpm(p, 15, &rpm));
    assert(rpm == 0);
}

void test_upper_bits_of_third_byte_ignored() {
    uint8_t p[15] = {0};
    p[12] = 0x58; p[13] = 0x02; p[14] = 0xFC;  // bits above 18 are not RPM
    uint16_t rpm = 0;
    assert(parseEscStatusRpm(p, 15, &rpm));
    assert(rpm == 600);
}

int main() {
    test_short_frame_keeps_previous_value();
    test_positive_rpm();
    test_negative_rpm_is_absolute();
    test_real_zero_is_true();
    test_upper_bits_of_third_byte_ignored();
    cout << "EscStatusRpmTest: all passed" << endl;
    return 0;
}
