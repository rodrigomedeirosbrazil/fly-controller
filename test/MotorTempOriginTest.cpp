// test/MotorTempOriginTest.cpp
#include <cassert>
#include <cstring>
#include <iostream>
#include "../src/Telemetry/MotorTempOrigin.h"
using namespace std;

// Pins the numeric layout: `None` is the sentinel aliased by
// SignalArmContract's tag 0, so it must stay 0 (see the assert in the header).
static_assert((uint8_t)MotorTempOrigin::None == 0, "None must stay 0");

void test_codes_match_table() {
    assert(strcmp(motorTempOriginCode(MotorTempOrigin::Can), "can") == 0);
    assert(strcmp(motorTempOriginCode(MotorTempOrigin::Ntc), "ntc") == 0);
    assert(motorTempOriginCode(MotorTempOrigin::None) == nullptr);
    cout << "PASS: codes match the design table\n";
}

void test_none_is_absent_not_a_code() {
    // The page omits the field for None; a raw code would leak a meaningless
    // value into the JSON.
    for (MotorTempOrigin o : { MotorTempOrigin::Can, MotorTempOrigin::Ntc }) {
        assert(motorTempOriginCode(o) != nullptr);
    }
    cout << "PASS: only Can/Ntc yield a code, None yields nothing\n";
}

int main() {
    test_codes_match_table();
    test_none_is_absent_not_a_code();
    cout << "MotorTempOriginTest: all passed" << endl;
    return 0;
}
