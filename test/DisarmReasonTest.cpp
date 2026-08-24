// test/DisarmReasonTest.cpp
#include <cassert>
#include <cstring>
#include <iostream>
#include "../src/DisarmReason.h"
using namespace std;

void test_codes_match_table() {
    assert(strcmp(disarmReasonCode(DisarmReason::None), "") == 0);
    assert(strcmp(disarmReasonCode(DisarmReason::Manual), "MANUAL") == 0);
    assert(strcmp(disarmReasonCode(DisarmReason::ThrottleWiredInvalid), "THR ERR") == 0);
    assert(strcmp(disarmReasonCode(DisarmReason::ThrottleLinkLost), "LINK ERR") == 0);
    assert(strcmp(disarmReasonCode(DisarmReason::MotorTempLost), "MOT ERR") == 0);
    assert(strcmp(disarmReasonCode(DisarmReason::EscTempLost), "ESC ERR") == 0);
    assert(strcmp(disarmReasonCode(DisarmReason::BatteryVoltageLost), "BATT ERR") == 0);
    assert(strcmp(disarmReasonCode(DisarmReason::MotorTempSourceChanged), "MOT SRC") == 0);
    cout << "PASS: codes match the design table\n";
}

void test_codes_fit_xctrack_field_width() {
    // XCTRACK's system-status field must hold every code, 8 chars max.
    const DisarmReason all[] = {
        DisarmReason::None, DisarmReason::Manual, DisarmReason::ThrottleWiredInvalid,
        DisarmReason::ThrottleLinkLost, DisarmReason::MotorTempLost,
        DisarmReason::EscTempLost, DisarmReason::BatteryVoltageLost,
        DisarmReason::MotorTempSourceChanged,
    };
    for (DisarmReason r : all) {
        assert(strlen(disarmReasonCode(r)) <= 8);
    }
    cout << "PASS: every code fits the 8-char field width\n";
}

int main() {
    test_codes_match_table();
    test_codes_fit_xctrack_field_width();
    cout << "DisarmReasonTest: all passed" << endl;
    return 0;
}
