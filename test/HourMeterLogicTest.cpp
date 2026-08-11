// test/HourMeterLogicTest.cpp
// Host-only unit test for the pure flight-time state machine. Compile with:
//   g++ -std=c++17 test/HourMeterLogicTest.cpp -o /tmp/hourmeter_test
#include <iostream>
#include <cassert>
#include "../src/HourMeter/HourMeterLogic.h"
using namespace std;

void test_initial_state() {
    HourMeterLogic l;
    assert(l.getSessionSec(0) == 0);
    assert(l.getHourMeterSec(0) == 0);
    cout << "PASS: initial state zeroed\n";
}

void test_counts_only_while_running() {
    HourMeterLogic l;
    l.tick(true, true, 0);            // running starts
    assert(l.getSessionSec(1000) == 1);
    assert(l.getSessionSec(2500) == 2);
    assert(l.getSessionSec(3000) == 3);
    cout << "PASS: counts only while armed && motor running\n";
}

void test_pauses_on_disarm() {
    HourMeterLogic l;
    l.tick(true, true, 0);
    l.tick(false, true, 2000);        // disarm -> fold 2s, pause
    assert(l.getSessionSec(2500) == 2);
    l.tick(false, false, 3000);       // still disarmed
    assert(l.getSessionSec(3500) == 2);
    cout << "PASS: pauses on disarm and keeps the value\n";
}

void test_pauses_on_motor_stop_while_armed() {
    HourMeterLogic l;
    l.tick(true, true, 0);
    l.tick(true, false, 1000);        // motor stops, still armed -> pause
    assert(l.getSessionSec(1500) == 1);
    l.tick(true, true, 2000);         // motor restarts -> resume
    assert(l.getSessionSec(3000) == 2);
    cout << "PASS: pauses on motor stop, resumes on restart\n";
}

void test_survives_disarm_ream_cycles() {
    HourMeterLogic l;
    l.tick(true, true, 0);
    l.tick(false, true, 1000);        // disarm after 1s
    assert(l.getSessionSec(1500) == 1);
    l.tick(true, true, 2000);         // re-arm -> resume from 1
    assert(l.getSessionSec(3000) == 2);
    l.tick(true, false, 3000);        // motor stop, still armed
    assert(l.getSessionSec(3500) == 2);
    l.tick(true, true, 4000);         // motor restarts -> resume from 2
    assert(l.getSessionSec(5000) == 3);
    cout << "PASS: accumulates across disarm/re-arm and motor stops\n";
}

void test_manual_reset_while_running() {
    HourMeterLogic l;
    l.tick(true, true, 0);
    l.tick(true, true, 2000);
    assert(l.getSessionSec(2000) == 2);
    l.requestReset();
    l.tick(true, true, 2500);         // reset applied; interval re-seeded at 2500
    assert(l.getSessionSec(2600) == 0);
    assert(l.getSessionSec(3500) == 1);
    cout << "PASS: manual reset while running\n";
}

void test_manual_reset_while_paused() {
    HourMeterLogic l;
    l.tick(true, true, 0);
    l.tick(false, true, 1000);
    assert(l.getSessionSec(1500) == 1);
    l.requestReset();
    l.tick(false, true, 2000);        // reset applied while paused
    assert(l.getSessionSec(3000) == 0);
    l.tick(true, true, 3500);         // resume from 0
    assert(l.getSessionSec(4500) == 1);
    cout << "PASS: manual reset while paused\n";
}

void test_reset_does_not_touch_hour_meter() {
    HourMeterLogic l;
    l.tick(true, true, 0);
    l.requestReset();
    l.tick(true, true, 5000);         // session reset; hour meter keeps counting
    assert(l.getSessionSec(6000) == 1);
    assert(l.getHourMeterSec(6000) == 6);
    cout << "PASS: manual reset leaves the persistent hour meter untouched\n";
}

void test_hour_meter_accumulates_running_time() {
    HourMeterLogic l;
    l.tick(true, true, 0);
    l.tick(true, false, 6000);        // motor stops -> fold 6s
    assert(l.getHourMeterSec(7000) == 6);
    l.tick(true, true, 7000);         // motor restarts
    assert(l.getHourMeterSec(9000) == 8);
    cout << "PASS: hour meter accumulates motor-run seconds\n";
}

void test_survives_millis_rollover() {
    HourMeterLogic l;
    uint32_t nearMax = 0xFFFFFFF0u;   // 15ms before wraparound
    l.tick(true, true, nearMax);
    uint32_t afterWrap = 500u;        // real elapsed: 515ms
    assert(l.getSessionSec(afterWrap) == 0);
    uint32_t later = 3000u;           // real elapsed: 3015ms
    assert(l.getSessionSec(later) == 3);
    cout << "PASS: survives millis() rollover\n";
}

int main() {
    test_initial_state();
    test_counts_only_while_running();
    test_pauses_on_disarm();
    test_pauses_on_motor_stop_while_armed();
    test_survives_disarm_ream_cycles();
    test_manual_reset_while_running();
    test_manual_reset_while_paused();
    test_reset_does_not_touch_hour_meter();
    test_hour_meter_accumulates_running_time();
    test_survives_millis_rollover();
    cout << "HourMeterLogicTest: all passed" << endl;
    return 0;
}
