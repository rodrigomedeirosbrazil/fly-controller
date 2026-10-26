#include <cassert>
#include <iostream>
#include "../src/Hobbywing/HobbywingCanParser.h"
using namespace std;
using namespace HobbywingCanParser;

// StatusMsg1: RPM 0x1234 little-endian, byte 5 bit 7 = CCW, tail 0xC0
static const uint8_t S1_TEMPLATE[7] = {0x34, 0x12, 0, 0, 0, 0x80, 0xC0};
// StatusMsg2: voltage 0x01F4 (ignored), current 0x0258 = 600 dA, temp 45, tail 0xC1
static const uint8_t S2_TEMPLATE[6] = {0xF4, 0x01, 0x58, 0x02, 45, 0xC1};

static const uint16_t RPM_SENTINEL = 0xBEEF;
static const uint32_t CURRENT_SENTINEL = 0xDEADBEEF;
static const uint8_t TEMP_SENTINEL = 0x7B;

void test_status1_decodes_rpm_and_direction() {
    uint8_t s1[7];
    for (int i = 0; i < 7; i++) s1[i] = S1_TEMPLATE[i];
    StatusMsg1 out = {0, false};
    assert(parseStatusMsg1(s1, 7, out));
    assert(out.rpm == 0x1234);
    assert(out.isCcw == true);

    s1[5] = 0x00;
    assert(parseStatusMsg1(s1, 7, out));
    assert(out.rpm == 0x1234);
    assert(out.isCcw == false);
    cout << "PASS: StatusMsg1 decodes RPM and direction\n";
}

void test_status1_rejects_wrong_length() {
    StatusMsg1 out = {RPM_SENTINEL, true};
    assert(!parseStatusMsg1(S1_TEMPLATE, 6, out));
    assert(!parseStatusMsg1(S1_TEMPLATE, 8, out));
    assert(out.rpm == RPM_SENTINEL);
    assert(out.isCcw == true);
    cout << "PASS: StatusMsg1 rejects a wrong length and leaves out untouched\n";
}

void test_status1_rejects_multi_frame_tail() {
    uint8_t s1[7];
    for (int i = 0; i < 7; i++) s1[i] = S1_TEMPLATE[i];
    StatusMsg1 out = {RPM_SENTINEL, true};
    s1[6] = 0x80;  // start only
    assert(!parseStatusMsg1(s1, 7, out));
    s1[6] = 0x40;  // end only
    assert(!parseStatusMsg1(s1, 7, out));
    assert(out.rpm == RPM_SENTINEL);
    assert(out.isCcw == true);
    cout << "PASS: StatusMsg1 rejects multi-frame tails\n";
}

void test_status2_decodes_current_and_temp() {
    StatusMsg2 out = {CURRENT_SENTINEL, TEMP_SENTINEL};
    assert(parseStatusMsg2(S2_TEMPLATE, 6, out));
    assert(out.batteryCurrentMilliAmps == 60000);
    assert(out.escTemperatureCelsius == 45);
    cout << "PASS: StatusMsg2 decodes current and temperature\n";
}

void test_status2_negative_current_clamps_to_zero() {
    uint8_t s2[6];
    for (int i = 0; i < 6; i++) s2[i] = S2_TEMPLATE[i];
    s2[2] = 0x38;
    s2[3] = 0xFF;  // -200 dA
    StatusMsg2 out = {CURRENT_SENTINEL, TEMP_SENTINEL};
    assert(parseStatusMsg2(s2, 6, out));
    assert(out.batteryCurrentMilliAmps == 0);
    assert(out.escTemperatureCelsius == 45);
    cout << "PASS: StatusMsg2 negative current clamps to zero\n";
}

void test_status2_rejects_wrong_length() {
    uint8_t s2[7] = {0xF4, 0x01, 0x58, 0x02, 45, 0x00, 0xC1};
    StatusMsg2 out = {CURRENT_SENTINEL, TEMP_SENTINEL};
    assert(!parseStatusMsg2(s2, 7, out));
    assert(!parseStatusMsg2(S2_TEMPLATE, 5, out));
    assert(out.batteryCurrentMilliAmps == CURRENT_SENTINEL);
    assert(out.escTemperatureCelsius == TEMP_SENTINEL);
    cout << "PASS: StatusMsg2 rejects a wrong length and leaves out untouched\n";
}

void test_status2_rejects_multi_frame_tail() {
    uint8_t s2[6];
    for (int i = 0; i < 6; i++) s2[i] = S2_TEMPLATE[i];
    s2[5] = 0xA0;
    StatusMsg2 out = {CURRENT_SENTINEL, TEMP_SENTINEL};
    assert(!parseStatusMsg2(s2, 6, out));
    assert(out.batteryCurrentMilliAmps == CURRENT_SENTINEL);
    assert(out.escTemperatureCelsius == TEMP_SENTINEL);
    cout << "PASS: StatusMsg2 rejects a multi-frame tail\n";
}

void test_rejects_empty_frame() {
    StatusMsg1 o1 = {RPM_SENTINEL, true};
    StatusMsg2 o2 = {CURRENT_SENTINEL, TEMP_SENTINEL};
    assert(!parseStatusMsg1(S1_TEMPLATE, 0, o1));
    assert(!parseStatusMsg2(S2_TEMPLATE, 0, o2));
    assert(o1.rpm == RPM_SENTINEL);
    assert(o2.batteryCurrentMilliAmps == CURRENT_SENTINEL);
    cout << "PASS: both parsers reject an empty frame\n";
}

void test_esc_temp_classification() {
    assert(classifyEscTemp(false, 45, 120) == SignalState::Stale);
    assert(classifyEscTemp(true, 120, 120) == SignalState::Valid);
    assert(classifyEscTemp(true, 121, 120) == SignalState::Invalid);
    assert(classifyEscTemp(true, 255, 120) == SignalState::Invalid);
    assert(classifyEscTemp(true, 0, 120) == SignalState::Valid);
    cout << "PASS: ESC temperature classification\n";
}

int main() {
    test_status1_decodes_rpm_and_direction();
    test_status1_rejects_wrong_length();
    test_status1_rejects_multi_frame_tail();
    test_status2_decodes_current_and_temp();
    test_status2_negative_current_clamps_to_zero();
    test_status2_rejects_wrong_length();
    test_status2_rejects_multi_frame_tail();
    test_rejects_empty_frame();
    test_esc_temp_classification();
    cout << "HobbywingCanParserTest: all passed" << endl;
    return 0;
}
