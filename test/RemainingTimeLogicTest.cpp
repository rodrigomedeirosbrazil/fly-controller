// test/RemainingTimeLogicTest.cpp
// Host-only unit test for the remaining-flight-time estimator. Compile with:
//   c++ -std=c++17 test/RemainingTimeLogicTest.cpp -o "${TMPDIR:-/tmp}/remaining_time_test"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>
#include "../src/RemainingTime/RemainingTimeLogic.h"
using namespace std;

// Drives the logic in 100 ms ticks. When drainCc is set, the coulomb count
// falls with the simulated current the way BatteryMonitor's would.
struct Sim {
    RemainingTimeLogic l;
    RemainingTimeInput in = {};
    double ccMah = 0;
    bool drainCc = false;
    uint16_t learned = 0;

    Sim(uint16_t referenceA, uint32_t startMs = 0) {
        l.seed(referenceA);
        in.nowMs = startMs;
        tickOnce();
    }

    void tickOnce() {
        in.ccRemainingMah = (uint32_t) ccMah;
        const uint16_t r = l.tick(in);
        if (r != 0) learned = r;
    }

    void run(uint32_t ms) {
        for (uint32_t t = 0; t < ms; t += 100) {
            in.nowMs += 100;
            if (drainCc && in.currentAvailable) {
                ccMah -= (double) in.currentMa * 100.0 / 3600000.0;
                if (ccMah < 0) ccMah = 0;
            }
            tickOnce();
        }
    }

    void motor(uint32_t currentMa) {
        in.armed = true;
        in.motorRunning = true;
        in.currentMa = currentMa;
    }

    void glide() {
        in.motorRunning = false;
        in.currentMa = 0;
    }
};

static bool near(uint32_t value, uint32_t expected, uint32_t tolerance) {
    return value + tolerance >= expected && value <= expected + tolerance;
}

static Sim coulombSim(uint16_t referenceA, double ccMah) {
    Sim s(referenceA);
    s.in.currentAvailable = true;
    s.in.voltageValid = true;
    s.ccMah = ccMah;
    s.l.seed(referenceA);
    s.tickOnce();
    return s;
}

void test_seed_gives_a_ground_estimate() {
    Sim s = coulombSim(90, 30000);
    // 30 Ah at 90 A = 20 min, before the motor has ever run.
    assert(s.l.quality() == RemainingTimeQuality::CoulombCount);
    assert(s.l.averageCurrentMa() == 90000);
    assert(s.l.remainingMin() == 20);
    cout << "PASS: the reference gives an estimate on the ground\n";
}

void test_block_needs_ten_seconds_of_motor_time() {
    Sim s = coulombSim(90, 30000);
    s.motor(200000);
    s.run(9900);
    assert(s.l.averageCurrentMa() == 90000);   // block not complete yet
    s.glide();
    s.run(60000);
    assert(s.l.averageCurrentMa() == 90000);   // a glide does not complete it
    s.motor(200000);
    s.run(100);                                // 10 s of motor time reached
    assert(near(s.l.averageCurrentMa(), 98795, 2));
    cout << "PASS: a block is 10 s of motor time and survives a glide\n";
}

void test_ema_time_constant_is_two_minutes() {
    Sim s = coulombSim(90, 30000);
    s.motor(150000);
    s.run(120000);                             // 12 blocks = one tau
    // 150 - 60 * e^-1 = 127.9 A
    assert(near(s.l.averageCurrentMa(), 127927, 5));
    cout << "PASS: step response matches tau = 120 s\n";
}

void test_average_frozen_while_motor_not_running() {
    Sim s = coulombSim(90, 30000);
    s.motor(150000);
    s.run(60000);
    const uint32_t before = s.l.averageCurrentMa();
    s.glide();
    s.in.currentMa = 3000;                     // idle draw while gliding
    s.run(300000);
    assert(s.l.averageCurrentMa() == before);
    s.in.armed = false;                        // and on the ground
    s.run(60000);
    assert(s.l.averageCurrentMa() == before);
    cout << "PASS: the average is frozen whenever the motor is not running\n";
}

void test_publishes_every_ten_seconds() {
    Sim s = coulombSim(90, 30000);
    assert(s.l.remainingMin() == 20);
    s.ccMah = 15000;
    s.run(9900);
    assert(s.l.remainingMin() == 20);          // held between publications
    s.run(100);
    assert(s.l.remainingMin() == 10);
    cout << "PASS: the published value changes at most every 10 s\n";
}

void test_quality_change_publishes_immediately() {
    Sim s = coulombSim(90, 30000);
    s.run(1000);
    s.in.currentAvailable = false;
    s.in.voltageUsableMah = 15000;
    s.run(100);
    assert(s.l.quality() == RemainingTimeQuality::Voltage);
    // Anchored on the last coulomb count, not on the voltage reading.
    assert(s.l.remainingMin() == 20);
    cout << "PASS: a quality change is published on the tick it happens\n";
}

void test_unavailable_without_current_or_voltage() {
    Sim s(90);
    assert(s.l.quality() == RemainingTimeQuality::Unavailable);
    assert(s.l.remainingMin() == 0);
    cout << "PASS: no current and no voltage is Unavailable\n";
}

void test_learning_blends_the_flight_average() {
    Sim s = coulombSim(90, 30000);
    s.motor(60000);
    s.run(200000);
    s.glide();
    s.in.armed = false;
    s.run(100);
    assert(s.learned == 75);                   // (90 + 60) / 2
    assert(s.l.referenceCurrentA() == 75);
    cout << "PASS: learning blends the flight average into the reference\n";
}

void test_learning_needs_three_minutes_of_motor() {
    Sim s = coulombSim(90, 30000);
    s.motor(60000);
    s.run(170000);
    s.glide();
    s.in.armed = false;
    s.run(100);
    assert(s.learned == 0);
    assert(s.l.referenceCurrentA() == 90);
    cout << "PASS: under 180 s of motor time teaches nothing\n";
}

void test_learning_skipped_after_current_loss() {
    Sim s = coulombSim(90, 30000);
    s.motor(60000);
    s.run(100000);
    s.in.currentAvailable = false;
    s.run(1000);
    s.in.currentAvailable = true;
    s.run(100000);
    s.glide();
    s.in.armed = false;
    s.run(100);
    assert(s.learned == 0);
    cout << "PASS: a flight that lost its current measurement teaches nothing\n";
}

void test_learning_clamps_to_the_valid_range() {
    Sim high = coulombSim(300, 30000);
    high.motor(400000);
    high.run(200000);
    high.glide();
    high.in.armed = false;
    high.run(100);
    assert(high.learned == 300);

    Sim low = coulombSim(10, 30000);
    low.motor(2000);
    low.run(200000);
    low.glide();
    low.in.armed = false;
    low.run(100);
    assert(low.learned == 0);
    cout << "PASS: the learned reference is clamped to 300 A at the top\n";
}

void test_learning_refuses_a_zero_reading_flight() {
    Sim s = coulombSim(90, 30000);
    s.motor(0);                                // fresh telemetry, but 0 mA
    s.run(200000);
    s.glide();
    s.in.armed = false;
    s.run(100);
    assert(s.learned == 0);
    assert(s.l.referenceCurrentA() == 90);
    cout << "PASS: a flight that averaged under 10 A teaches nothing\n";
}

void test_learning_refusal_boundary_is_ten_amps() {
    Sim below = coulombSim(90, 30000);
    below.motor(9000);
    below.run(200000);
    below.glide();
    below.in.armed = false;
    below.run(100);
    assert(below.learned == 0);              // 9 A: a sensor reading near zero

    Sim at = coulombSim(90, 30000);
    at.motor(10000);
    at.run(200000);
    at.glide();
    at.in.armed = false;
    at.run(100);
    assert(at.learned == 50);                // 10 A learns: (90 + 10) / 2
    cout << "PASS: learning refuses under 10 A and learns at 10 A\n";
}

void test_learning_counts_one_arm_session_only() {
    Sim s = coulombSim(90, 30000);
    s.motor(60000);
    s.run(100000);
    s.glide();
    s.in.armed = false;
    s.run(100);
    s.motor(60000);
    s.run(100000);
    s.glide();
    s.in.armed = false;
    s.run(100);
    assert(s.learned == 0);                    // 100 s + 100 s are not summed
    cout << "PASS: learning looks at one arm session at a time\n";
}

void test_learning_does_not_reseed_the_average() {
    Sim s = coulombSim(90, 30000);
    s.motor(60000);
    s.run(200000);
    const uint32_t before = s.l.averageCurrentMa();
    s.glide();
    s.in.armed = false;
    s.run(100);
    assert(s.learned == 75);
    assert(s.l.averageCurrentMa() == before);
    cout << "PASS: learning leaves the in-flight average alone\n";
}

void test_voltage_mode_before_any_anchor() {
    Sim s(75);
    s.in.voltageValid = true;
    s.in.voltageUsableMah = 20000;
    s.run(100);
    assert(s.l.quality() == RemainingTimeQuality::Voltage);
    assert(s.l.remainingMin() == 16);          // 20 Ah at 75 A
    cout << "PASS: voltage mode reads the instantaneous voltage before an anchor\n";
}

void test_voltage_mode_anchors_at_rest_and_dead_reckons() {
    Sim s(75);
    s.in.voltageValid = true;
    s.in.voltageUsableMah = 20000;
    s.run(30000);                              // settle -> anchor 20000 mAh
    s.motor(0);
    s.in.voltageUsableMah = 15000;             // sag under load
    s.run(60000);
    // 20000 - 75 A x 60 s (1250 mAh) = 18750 mAh -> 15 min
    assert(s.l.remainingMin() == 15);
    s.in.voltageUsableMah = 9000;
    s.run(540000);                             // 10 min of motor in total
    // 20000 - 12500 = 7500, below the loaded-voltage floor of 9000 -> 7 min
    assert(s.l.remainingMin() == 7);
    cout << "PASS: voltage mode anchors at rest, dead-reckons, floors at the loaded voltage\n";
}

void test_voltage_anchor_needs_a_full_settle() {
    Sim s(75);
    s.in.voltageValid = true;
    s.in.voltageUsableMah = 20000;
    s.run(20000);                              // only 20 s of rest
    s.motor(0);
    s.in.voltageUsableMah = 12000;
    s.run(60000);
    // No anchor yet, so the reading is used directly: 12 Ah at 75 A
    assert(s.l.remainingMin() == 9);
    s.glide();
    s.in.voltageUsableMah = 18000;             // recovered at rest
    s.run(30000);
    assert(s.l.remainingMin() == 14);          // anchored at 18000
    cout << "PASS: the voltage anchor waits for 30 s of continuous rest\n";
}

void test_switch_to_voltage_keeps_the_energy() {
    Sim s = coulombSim(90, 18000);
    s.motor(90000);
    s.run(10000);
    const uint16_t ccMinutes = s.l.remainingMin();
    assert(ccMinutes == 12);
    s.in.currentAvailable = false;
    s.in.voltageUsableMah = 12000;             // loaded voltage reads low
    s.run(100);
    assert(s.l.quality() == RemainingTimeQuality::Voltage);
    assert(s.l.remainingMin() == ccMinutes);   // no jump at the switch
    s.in.currentAvailable = true;
    s.run(100);
    assert(s.l.quality() == RemainingTimeQuality::CoulombCount);
    cout << "PASS: losing current mid-flight continues from the coulomb count\n";
}

void test_learning_skipped_after_current_loss_while_gliding() {
    Sim s = coulombSim(90, 30000);
    s.motor(60000);
    s.run(200000);
    s.glide();
    s.in.currentAvailable = false;             // lost during a glide, still armed
    s.run(10000);
    s.in.currentAvailable = true;
    s.run(1000);
    s.in.armed = false;
    s.run(100);
    assert(s.learned == 0);
    cout << "PASS: a current loss while armed vetoes learning even off-motor\n";
}

void test_switch_after_long_rest_keeps_coulomb_energy() {
    Sim s = coulombSim(90, 18000);
    s.motor(90000);
    s.run(10000);
    assert(s.l.remainingMin() == 12);
    s.glide();
    s.run(40000);                              // well past the 30 s settle
    s.in.currentAvailable = false;
    s.in.voltageUsableMah = 12000;
    s.run(100);
    assert(s.l.quality() == RemainingTimeQuality::Voltage);
    assert(s.l.remainingMin() == 12);          // CC energy, not the voltage's 8
    s.run(30000);                              // a full settle after the loss
    assert(s.l.remainingMin() == 8);           // now anchored on 12000 mAh
    cout << "PASS: a loss after a long rest keeps the coulomb energy until a fresh settle\n";
}

void test_empty_battery_reads_zero() {
    Sim s = coulombSim(90, 0);
    assert(s.l.remainingMin() == 0);
    assert(s.l.quality() == RemainingTimeQuality::CoulombCount);
    cout << "PASS: an empty battery reads 0 min\n";
}

void test_tiny_average_never_divides_by_zero() {
    Sim s = coulombSim(10, 30000);
    s.motor(0);                                // engaged but drawing nothing
    s.run(3600000);
    // The EMA has decayed to ~0; the divisor floors at 1 A: 30 Ah / 1 A
    assert(s.l.remainingMin() == 1800);
    cout << "PASS: a near-zero average floors at 1 A\n";
}

void test_millis_rollover() {
    Sim s(90, 0xFFFFFFFFu - 5000);
    s.in.currentAvailable = true;
    s.ccMah = 30000;
    s.motor(200000);
    s.run(10000);
    assert(near(s.l.averageCurrentMa(), 98795, 2));
    cout << "PASS: blocks and publication survive millis() rollover\n";
}

void test_flight_profile_shape() {
    // Shaped like the 2026-04-25 Tmotor log: short takeoff spike, a climb
    // decaying into cruise, a glide, then a climb in the middle of the flight.
    Sim s = coulombSim(90, 32000);
    s.drainCc = true;

    s.run(60000);
    const uint16_t ground = s.l.remainingMin();

    s.motor(180000);
    s.run(45000);
    const uint16_t afterTakeoff = s.l.remainingMin();

    for (uint32_t i = 0; i < 180; i++) {       // 3 min, 100 A falling to 70 A
        s.in.currentMa = 100000 - i * 30000 / 180;
        s.run(1000);
    }
    const uint16_t afterClimb = s.l.remainingMin();

    s.in.currentMa = 55000;
    s.run(360000);
    const uint16_t afterCruise = s.l.remainingMin();

    s.glide();
    s.run(10000);                              // let a stale publication age out
    vector<uint16_t> glideValues;
    for (int i = 0; i < 170; i++) {
        s.run(1000);
        glideValues.push_back(s.l.remainingMin());
    }

    s.motor(120000);
    s.run(60000);
    const uint16_t afterMidClimb = s.l.remainingMin();

    assert(afterTakeoff < ground);             // takeoff pulls it down
    assert(afterCruise > afterClimb);          // cruise brings it back up
    for (uint16_t v : glideValues) {
        assert(v == glideValues.front());      // a glide leaves it alone
    }
    assert(afterMidClimb < glideValues.front()); // a climb pulls it down again
    cout << "PASS: flight profile: " << ground << " -> " << afterTakeoff << " -> "
         << afterClimb << " -> " << afterCruise << " -> " << glideValues.front()
         << " -> " << afterMidClimb << " min\n";
}

int main() {
    test_seed_gives_a_ground_estimate();
    test_block_needs_ten_seconds_of_motor_time();
    test_ema_time_constant_is_two_minutes();
    test_average_frozen_while_motor_not_running();
    test_publishes_every_ten_seconds();
    test_quality_change_publishes_immediately();
    test_unavailable_without_current_or_voltage();
    test_learning_blends_the_flight_average();
    test_learning_needs_three_minutes_of_motor();
    test_learning_skipped_after_current_loss();
    test_learning_clamps_to_the_valid_range();
    test_learning_refuses_a_zero_reading_flight();
    test_learning_refusal_boundary_is_ten_amps();
    test_learning_counts_one_arm_session_only();
    test_learning_does_not_reseed_the_average();
    test_voltage_mode_before_any_anchor();
    test_voltage_mode_anchors_at_rest_and_dead_reckons();
    test_voltage_anchor_needs_a_full_settle();
    test_switch_to_voltage_keeps_the_energy();
    test_learning_skipped_after_current_loss_while_gliding();
    test_switch_after_long_rest_keeps_coulomb_energy();
    test_empty_battery_reads_zero();
    test_tiny_average_never_divides_by_zero();
    test_millis_rollover();
    test_flight_profile_shape();
    cout << "All RemainingTimeLogic tests passed\n";
    return 0;
}
