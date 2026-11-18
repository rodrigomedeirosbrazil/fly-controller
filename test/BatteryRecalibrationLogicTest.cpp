// test/BatteryRecalibrationLogicTest.cpp
// Host-only unit test for BatteryRecalibrationLogic. Compile with:
//   c++ -std=c++17 test/BatteryRecalibrationLogicTest.cpp -o "${TMPDIR:-/tmp}/battery_recal_test"
#include <cassert>
#include <cstdint>
#include <iostream>
#include "../src/BatteryMonitor/BatteryRecalibrationLogic.h"
using namespace std;

static const uint32_t STEP_MS = 100;

struct Sim {
    BatteryRecalibrationLogic l;
    BatteryRecalibrationInput in;
    uint32_t startMs;
    BatteryRecalibration last = BatteryRecalibration::None;
    uint32_t lastAtMs = 0;  // ms since the start of the most recent run() that fired

    Sim(uint32_t start = 0) : startMs(start) {
        in.nowMs = start;
        in.motorRunning = false;
        in.voltageValid = true;
    }

    // Ticks for durationMs; returns the first non-None result (None if nothing
    // fired) and records when, relative to the start of this run.
    BatteryRecalibration run(uint32_t durationMs) {
        BatteryRecalibration first = BatteryRecalibration::None;
        const uint32_t from = in.nowMs;
        for (uint32_t t = 0; t < durationMs; t += STEP_MS) {
            const BatteryRecalibration r = l.tick(in);
            if (r != BatteryRecalibration::None && first == BatteryRecalibration::None) {
                first = r;
                lastAtMs = in.nowMs - from;
            }
            in.nowMs += STEP_MS;
        }
        last = first;
        return first;
    }
};

static Sim bootedSim() {
    Sim s;
    assert(s.run(3000) == BatteryRecalibration::Boot);
    return s;
}

void test_boot_fires_after_two_seconds_quiet() {
    Sim s;
    assert(s.l.bootPending());
    assert(s.run(5000) == BatteryRecalibration::Boot);
    assert(s.lastAtMs >= BATTERY_BOOT_SETTLE_MS && s.lastAtMs < BATTERY_BOOT_SETTLE_MS + 100);
    assert(!s.l.bootPending());
    cout << "PASS: boot fires after 2 s quiet\n";
}

void test_boot_fires_only_once() {
    Sim s = bootedSim();
    assert(s.run(600000) == BatteryRecalibration::None);
    cout << "PASS: boot fires only once, stop event consumed\n";
}

void test_boot_waits_for_valid_voltage() {
    Sim s;
    s.in.voltageValid = false;
    assert(s.run(60000) == BatteryRecalibration::None);
    assert(s.l.bootPending());
    s.in.voltageValid = true;
    assert(s.run(5000) == BatteryRecalibration::Boot);
    assert(s.lastAtMs >= BATTERY_BOOT_SETTLE_MS && s.lastAtMs < BATTERY_BOOT_SETTLE_MS + 100);
    cout << "PASS: boot waits for a valid voltage\n";
}

void test_boot_waits_while_motor_running() {
    Sim s;
    s.in.motorRunning = true;
    assert(s.run(300000) == BatteryRecalibration::None);
    assert(s.l.bootPending());
    s.in.motorRunning = false;
    assert(s.run(5000) == BatteryRecalibration::Boot);
    assert(s.lastAtMs >= BATTERY_BOOT_SETTLE_MS && s.lastAtMs < BATTERY_BOOT_SETTLE_MS + 100);
    cout << "PASS: boot waits while the motor runs\n";
}

void test_rest_fires_after_thirty_seconds_stopped() {
    Sim s = bootedSim();
    s.in.motorRunning = true;
    assert(s.run(60000) == BatteryRecalibration::None);
    s.in.motorRunning = false;
    assert(s.run(40000) == BatteryRecalibration::Rest);
    assert(s.lastAtMs >= BATTERY_REST_SETTLE_MS - 100 && s.lastAtMs <= BATTERY_REST_SETTLE_MS + 100);
    cout << "PASS: rest fires 30 s after the stop\n";
}

void test_rest_fires_once_per_stop() {
    Sim s = bootedSim();
    s.in.motorRunning = true;
    s.run(60000);
    s.in.motorRunning = false;
    assert(s.run(40000) == BatteryRecalibration::Rest);
    assert(s.run(600000) == BatteryRecalibration::None);
    cout << "PASS: rest fires once per stop\n";
}

void test_rest_rearms_after_motor_runs() {
    Sim s = bootedSim();
    s.in.motorRunning = true;
    s.run(60000);
    s.in.motorRunning = false;
    assert(s.run(40000) == BatteryRecalibration::Rest);
    s.in.motorRunning = true;
    assert(s.run(1000) == BatteryRecalibration::None);
    s.in.motorRunning = false;
    assert(s.run(40000) == BatteryRecalibration::Rest);
    assert(s.lastAtMs >= BATTERY_REST_SETTLE_MS - 100 && s.lastAtMs <= BATTERY_REST_SETTLE_MS + 100);
    cout << "PASS: rest re-arms after the motor runs\n";
}

void test_motor_blip_restarts_rest_wait() {
    Sim s = bootedSim();
    s.in.motorRunning = true;
    s.run(60000);
    s.in.motorRunning = false;
    assert(s.run(20000) == BatteryRecalibration::None);
    s.in.motorRunning = true;
    assert(s.run(1000) == BatteryRecalibration::None);
    s.in.motorRunning = false;
    assert(s.run(29000) == BatteryRecalibration::None);   // not early
    assert(s.run(2000) == BatteryRecalibration::Rest);
    cout << "PASS: a motor blip restarts the rest wait\n";
}

void test_invalid_voltage_restarts_rest_wait() {
    Sim s = bootedSim();
    s.in.motorRunning = true;
    s.run(60000);
    s.in.motorRunning = false;
    assert(s.run(20000) == BatteryRecalibration::None);
    s.in.voltageValid = false;
    assert(s.run(1000) == BatteryRecalibration::None);
    s.in.voltageValid = true;
    assert(s.run(29000) == BatteryRecalibration::None);   // not early
    assert(s.run(2000) == BatteryRecalibration::Rest);
    cout << "PASS: an invalid voltage restarts the rest wait\n";
}

void test_millis_rollover() {
    Sim s(0xFFFFFFFFu - 1000);
    assert(s.run(5000) == BatteryRecalibration::Boot);
    assert(s.lastAtMs >= BATTERY_BOOT_SETTLE_MS && s.lastAtMs < BATTERY_BOOT_SETTLE_MS + 100);
    cout << "PASS: millis() rollover\n";
}

int main() {
    test_boot_fires_after_two_seconds_quiet();
    test_boot_fires_only_once();
    test_boot_waits_for_valid_voltage();
    test_boot_waits_while_motor_running();
    test_rest_fires_after_thirty_seconds_stopped();
    test_rest_fires_once_per_stop();
    test_rest_rearms_after_motor_runs();
    test_motor_blip_restarts_rest_wait();
    test_invalid_voltage_restarts_rest_wait();
    test_millis_rollover();
    cout << "All BatteryRecalibrationLogic tests passed\n";
    return 0;
}
