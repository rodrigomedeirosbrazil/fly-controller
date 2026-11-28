#include <iostream>
#include <cassert>
using namespace std;

#include "../src/ThrottleOutput/ThrottleOutputLogic.h"

static ThrottleOutputConfig tmotorCfg() {
    ThrottleOutputConfig c{};
    c.softStart = true; c.wakeWaitMs = 0; c.wakeLevelPpm = 0;
    c.startRampTopPpm = 200000; c.startRampRatePpmPerS = 66667;
    c.releaseByRpm = true; c.releaseRpm = 600; c.releaseMaxMs = 4000;
    c.runningOffsetPpm = 150000; c.decelRatePpmPerS = 1000000;
    c.stopByRpm = true; c.rpmStopConfirmMs = 500; c.coastStopMs = 3000;
    return c;
}
static ThrottleOutputConfig xagCfg() {
    ThrottleOutputConfig c{};
    c.softStart = true; c.wakeWaitMs = 1500; c.wakeLevelPpm = 50000;
    c.startRampTopPpm = 200000; c.startRampRatePpmPerS = 50000;
    c.releaseByRpm = false; c.releaseRpm = 0; c.releaseMaxMs = 0;
    c.runningOffsetPpm = 0; c.decelRatePpmPerS = 1000000;
    c.stopByRpm = false; c.rpmStopConfirmMs = 0; c.coastStopMs = 800;
    return c;
}
static ThrottleOutputConfig hobbywingCfg() {
    ThrottleOutputConfig c = xagCfg();
    c.softStart = false; c.wakeWaitMs = 0; c.wakeLevelPpm = 0;
    return c;
}

struct H {
    ThrottleOutputLogic l;
    uint32_t now;
    ThrottleOutputInput in{};
    H(const ThrottleOutputConfig& c, uint32_t start = 1000) : l(c), now(start) {
        in.calibrated = true; in.armed = true; in.demandPpm = 0;
        in.ceilingPpm = 1000000; in.disarmScalePct = 100; in.rpmFresh = false; in.rpm = 0;
    }
    uint32_t tick(uint32_t dtMs = 10) {
        now += dtMs; in.nowMs = now; return l.update(in);
    }
    void run(int n) { for (int i = 0; i < n; ++i) tick(); }
};

// Brings a T-Motor stage into Running at demand 1e6 (released by RPM).
static void toRunning(H& h) {
    h.in.demandPpm = 1000000; h.tick(0);
    h.run(10);
    h.in.rpmFresh = true; h.in.rpm = 600; h.tick();
    assert(h.l.state() == ThrottleOutputState::Running);
}

static void test_uncalibratedIsZero() {
    H h(tmotorCfg()); h.in.calibrated = false; h.in.demandPpm = 1000000;
    for (int i = 0; i < 10; ++i) assert(h.tick() == 0);
    assert(h.l.state() == ThrottleOutputState::Stopped);
    cout << "PASS: uncalibratedIsZero\n";
}
static void test_gateReleasedStaysStopped() {
    H h(tmotorCfg());
    for (int i = 0; i < 1000; ++i) assert(h.tick() == 0);
    assert(h.l.state() == ThrottleOutputState::Stopped);
    cout << "PASS: gateReleasedStaysStopped\n";
}
static void tmotorRampScenario(uint32_t start) {
    H h(tmotorCfg(), start); h.in.demandPpm = 1000000;
    h.tick(0);
    for (int i = 0; i < 150; ++i) { uint32_t o = h.tick(); assert(o <= 200000); }
    uint32_t o = h.l.output();
    assert(o >= 99000 && o <= 101000);
    assert(h.l.state() == ThrottleOutputState::Starting);
}
static void test_tmotorRampsFromZero() { tmotorRampScenario(1000); cout << "PASS: tmotorRampsFromZero\n"; }
static void test_tmotorReleasesByRpm() {
    H h(tmotorCfg()); h.in.demandPpm = 1000000; h.tick(0); h.run(100);
    h.in.rpmFresh = true; h.in.rpm = 600;
    assert(h.tick() == 1000000);
    assert(h.l.state() == ThrottleOutputState::Running);
    cout << "PASS: tmotorReleasesByRpm\n";
}
static void test_tmotorTimeReleaseOnlyAfterRampTop() {
    H h(tmotorCfg()); h.in.demandPpm = 1000000; h.tick(0); h.run(390);
    assert(h.l.state() == ThrottleOutputState::Starting);
    h.run(11);
    assert(h.l.state() == ThrottleOutputState::Running);
    cout << "PASS: tmotorTimeReleaseOnlyAfterRampTop\n";
}
static void test_lowDemandStaysStartingWithoutOffset() {
    H h(tmotorCfg()); h.in.demandPpm = 100000; h.tick(0); h.run(2000);
    assert(h.l.state() == ThrottleOutputState::Starting);
    assert(h.l.output() == 100000);
    cout << "PASS: lowDemandStaysStartingWithoutOffset\n";
}
static void test_xagWakeThenRamp() {
    H h(xagCfg()); h.in.demandPpm = 1000000; h.tick(0);
    for (int i = 0; i < 100; ++i) assert(h.tick() == 50000);
    h.run(100);  // t = 2.0 s
    uint32_t o = h.l.output();
    assert(o >= 73500 && o <= 76500);
    h.run(280);
    assert(h.l.state() == ThrottleOutputState::Running);
    assert(h.l.output() == 1000000);
    cout << "PASS: xagWakeThenRamp\n";
}
static void test_hobbywingNoSoftStart() {
    H h(hobbywingCfg()); h.in.demandPpm = 600000; h.tick(0);
    assert(h.l.state() == ThrottleOutputState::Running);
    assert(h.l.output() == 600000);
    cout << "PASS: hobbywingNoSoftStart\n";
}
static void test_runningOffset() {
    H h(tmotorCfg()); toRunning(h);
    h.in.rpmFresh = false; h.in.demandPpm = 0; h.run(200);
    assert(h.l.output() == 0 && h.l.state() == ThrottleOutputState::Running);
    h.in.demandPpm = 10000;
    assert(h.tick() == 158500);
    cout << "PASS: runningOffset\n";
}
static void test_instantRiseInRunning() {
    H h(tmotorCfg()); toRunning(h);
    h.in.demandPpm = 0; h.run(70);  // decel down to ~300000
    uint32_t o = h.l.output(); assert(o < 400000 && o > 200000);
    h.in.demandPpm = 1000000;
    assert(h.tick() == 1000000);
    cout << "PASS: instantRiseInRunning\n";
}
static void test_decelRate() {
    H h(tmotorCfg()); toRunning(h);
    h.in.demandPpm = 0; h.run(50);
    uint32_t o = h.l.output(); assert(o >= 490000 && o <= 510000);
    h.run(50);
    assert(h.l.output() == 0);
    cout << "PASS: decelRate\n";
}
static void test_reversalMidDecel() {
    H h(tmotorCfg()); toRunning(h);
    h.in.demandPpm = 0; h.run(30);
    h.in.demandPpm = 1000000;
    assert(h.tick() == 1000000);
    assert(h.l.state() == ThrottleOutputState::Running);
    cout << "PASS: reversalMidDecel\n";
}
static void test_rpmZeroUnderCommandStaysRunning() {
    H h(tmotorCfg()); toRunning(h);
    h.in.demandPpm = 600000; h.run(50);
    uint32_t o = h.l.output(); assert(o == 660000);
    h.in.rpmFresh = true; h.in.rpm = 0;
    for (int i = 0; i < 500; ++i) assert(h.tick() == o);
    assert(h.l.state() == ThrottleOutputState::Running);
    cout << "PASS: rpmZeroUnderCommandStaysRunning\n";
}
static void stopByRpmScenario(uint32_t start) {
    H h(tmotorCfg(), start); h.in.demandPpm = 1000000; h.tick(0); h.run(10);
    h.in.rpmFresh = true; h.in.rpm = 600; h.tick();
    h.in.demandPpm = 0; h.run(100);
    assert(h.l.output() == 0);
    h.in.rpm = 0;
    h.run(40);
    assert(h.l.state() == ThrottleOutputState::Running);
    h.run(20);
    assert(h.l.state() == ThrottleOutputState::Stopped);
}
static void test_stopByRpmAfterRelease() { stopByRpmScenario(1000); cout << "PASS: stopByRpmAfterRelease\n"; }
static void test_staleRpmUsesCoastTime() {
    H h(tmotorCfg()); toRunning(h);
    h.in.rpmFresh = false; h.in.demandPpm = 0; h.run(100);
    assert(h.l.output() == 0);
    h.run(280);
    assert(h.l.state() == ThrottleOutputState::Running);
    h.run(40);
    assert(h.l.state() == ThrottleOutputState::Stopped);
    cout << "PASS: staleRpmUsesCoastTime\n";
}
static void test_dipDoesNotRestart() {
    H h(tmotorCfg()); toRunning(h);
    h.in.demandPpm = 0; h.in.rpm = 0; h.run(120);
    h.in.demandPpm = 500000; h.tick();
    assert(h.l.state() == ThrottleOutputState::Running);
    assert(h.l.output() == 150000 + 850000 / 2);
    cout << "PASS: dipDoesNotRestart\n";
}
static void test_restartAfterStopGoesThroughSoftStart() {
    H h(tmotorCfg()); toRunning(h);
    h.in.demandPpm = 0; h.in.rpm = 0; h.run(200);
    assert(h.l.state() == ThrottleOutputState::Stopped);
    h.in.demandPpm = 1000000; h.in.rpmFresh = false;
    for (int i = 0; i < 5; ++i) { h.tick(); assert(h.l.output() <= 5000); }
    assert(h.l.state() == ThrottleOutputState::Starting);
    cout << "PASS: restartAfterStopGoesThroughSoftStart\n";
}
static void test_disarmRampsFromLastOutput() {
    H h(tmotorCfg()); toRunning(h);
    h.in.ceilingPpm = 800000; h.tick();
    assert(h.l.output() == 800000 || h.l.output() >= 790000);
    h.run(20);
    assert(h.l.output() == 800000);
    h.in.ceilingPpm = 1000000; h.in.armed = false;
    uint32_t prev = h.l.output();
    for (int i = 0; i < 80; ++i) { uint32_t o = h.tick(); assert(o < prev || o == 0); prev = o; }
    assert(h.l.output() == 0 && h.l.state() == ThrottleOutputState::Stopped);
    cout << "PASS: disarmRampsFromLastOutput\n";
}
static void test_ceilingCapsFinalOutput() {
    H h(tmotorCfg()); toRunning(h);
    h.in.ceilingPpm = 600000; h.run(100);
    assert(h.l.output() == 600000 && h.l.isClipped());
    h.in.ceilingPpm = 1000000; h.tick();
    assert(h.l.output() == 1000000 && !h.l.isClipped());
    h.in.demandPpm = 0; h.in.ceilingPpm = 0; h.run(200);
    assert(!h.l.isClipped());
    cout << "PASS: ceilingCapsFinalOutput\n";
}
static void test_ceilingDropDecelerates() {
    H h(tmotorCfg()); toRunning(h);
    h.in.ceilingPpm = 400000; h.tick();
    assert(h.l.output() > 900000);
    h.run(60);
    assert(h.l.output() == 400000);
    cout << "PASS: ceilingDropDecelerates\n";
}
static void test_ceilingBelowRampTopHoldsStarting() {
    H h(tmotorCfg()); h.in.demandPpm = 1000000; h.in.ceilingPpm = 150000;
    h.tick(0); h.run(2000);
    assert(h.l.state() == ThrottleOutputState::Starting);
    assert(h.l.output() == 150000);
    h.in.rpmFresh = true; h.in.rpm = 600; h.tick();
    assert(h.l.state() == ThrottleOutputState::Running);
    assert(h.l.output() == 150000);
    cout << "PASS: ceilingBelowRampTopHoldsStarting\n";
}
static void test_disarmScale() {
    H h(tmotorCfg()); toRunning(h);
    h.in.disarmScalePct = 50; h.run(100);
    assert(h.l.output() == 500000);
    cout << "PASS: disarmScale\n";
}
static void test_rearmDuringDecelSlewsFromCurrentOutput() {
    H h(tmotorCfg()); toRunning(h);
    h.in.armed = false; h.run(30);
    uint32_t prev = h.l.output(); assert(prev >= 690000 && prev <= 710000);
    h.in.armed = true; h.in.rpmFresh = false; h.in.demandPpm = 1000000;
    for (int i = 0; i < 100; ++i) {
        uint32_t o = h.tick();
        assert(h.l.state() == ThrottleOutputState::Starting);
        if (prev > 200000) assert(o <= prev);
        prev = o;
    }
    assert(h.l.output() == 200000);
    for (int i = 0; i < 300; ++i) {
        assert(h.tick() <= 200000);
        assert(h.l.state() == ThrottleOutputState::Starting);
    }
    cout << "PASS: rearmDuringDecelSlewsFromCurrentOutput\n";
}
static void test_largeDtNeverUndershootsTarget() {
    H h(tmotorCfg()); toRunning(h);
    h.in.demandPpm = 0; // target 0 would be below; use a demand giving 300000 target
    h.in.demandPpm = 1000000;
    h.tick();
    // target 300000: offset 150000 + 850000*d/1e6 -> d = 176471
    h.in.demandPpm = 176471;
    uint32_t o = h.tick(2000);
    assert(o >= 299999 && o <= 300001);
    cout << "PASS: largeDtNeverUndershootsTarget\n";
}
static void test_largeDtNeverOvershootsRampTop() {
    H h(tmotorCfg()); h.in.demandPpm = 1000000; h.tick(0);
    assert(h.tick(10000) == 200000);
    cout << "PASS: largeDtNeverOvershootsRampTop\n";
}
static void test_millisRollover() {
    tmotorRampScenario(0xFFFFF000u);
    stopByRpmScenario(0xFFFFF000u);
    cout << "PASS: millisRollover\n";
}
static void test_outputToPwmMapping() {
    assert(outputToPwm(0, 1100, 1940) == 1100);
    assert(outputToPwm(1000000, 1100, 1940) == 1940);
    assert(outputToPwm(500000, 1100, 1940) == 1520);
    cout << "PASS: outputToPwmMapping\n";
}

int main() {
    test_uncalibratedIsZero(); test_gateReleasedStaysStopped(); test_tmotorRampsFromZero();
    test_tmotorReleasesByRpm(); test_tmotorTimeReleaseOnlyAfterRampTop();
    test_lowDemandStaysStartingWithoutOffset(); test_xagWakeThenRamp();
    test_hobbywingNoSoftStart(); test_runningOffset(); test_instantRiseInRunning();
    test_decelRate(); test_reversalMidDecel(); test_rpmZeroUnderCommandStaysRunning();
    test_stopByRpmAfterRelease(); test_staleRpmUsesCoastTime(); test_dipDoesNotRestart();
    test_restartAfterStopGoesThroughSoftStart(); test_disarmRampsFromLastOutput();
    test_ceilingCapsFinalOutput(); test_ceilingDropDecelerates();
    test_ceilingBelowRampTopHoldsStarting(); test_disarmScale();
    test_rearmDuringDecelSlewsFromCurrentOutput(); test_largeDtNeverUndershootsTarget();
    test_largeDtNeverOvershootsRampTop(); test_millisRollover(); test_outputToPwmMapping();
    return 0;
}
