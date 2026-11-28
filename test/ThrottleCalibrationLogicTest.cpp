#include <iostream>
#include <cassert>
#include <cstdlib>
using namespace std;

#include "../src/Throttle/ThrottleCalibrationLogic.h"

// Feeds a constant value every 10 ms from fromMs to toMs inclusive (wrapping
// uint32_t) and returns the last non-None event.
static CalibrationEvent feed(ThrottleCalibrationLogic &c, int value, uint32_t fromMs, uint32_t toMs) {
    CalibrationEvent last = CalibrationEvent::None;
    const uint32_t steps = (toMs - fromMs) / 10;
    for (uint32_t i = 0; i <= steps; ++i) {
        CalibrationEvent e = c.update(value, fromMs + i * 10);
        if (e != CalibrationEvent::None) last = e;
    }
    return last;
}

static void normalProcedure(uint32_t t0) {
    ThrottleCalibrationLogic c;
    assert(feed(c, 2400, t0, t0 + 3000) == CalibrationEvent::MaxCaptured && c.pinMax() == 2400);
    assert(c.step() == 1);
    assert(feed(c, 900, t0 + 3010, t0 + 6010) == CalibrationEvent::Complete);
    assert(c.isCalibrated() && c.pinMin() == 900 && c.pinMax() == 2400);
    // frozen after completion
    assert(feed(c, 1500, t0 + 6020, t0 + 12000) == CalibrationEvent::None);
    assert(c.pinMin() == 900 && c.pinMax() == 2400);
    c.reset();
    assert(!c.isCalibrated() && c.step() == 0);
}

int main() {
    normalProcedure(0);
    cout << "PASS: normalProcedure\n";

    {   // untouched throttle: silent restart, never calibrated
        ThrottleCalibrationLogic c;
        assert(feed(c, 900, 0, 3000) == CalibrationEvent::MaxCaptured && c.pinMax() == 900);
        uint32_t t = 3010;
        for (int round = 0; round < 3; ++round) {
            CalibrationEvent e = CalibrationEvent::None;
            for (uint32_t i = 0; i <= 300; ++i) {
                CalibrationEvent x = c.update(900, t + i * 10);
                assert(x != CalibrationEvent::RestartWithBeep);
                assert(x != CalibrationEvent::Complete);
                if (x != CalibrationEvent::None) e = x;
            }
            assert(e == CalibrationEvent::Restart);
            assert(c.step() == 0 && !c.isCalibrated());
            t += 3010;
            // re-capture max (rest value) then continue in step 1
            assert(feed(c, 900, t, t + 3000) == CalibrationEvent::MaxCaptured);
            t += 3010;
        }
    }
    cout << "PASS: untouchedThrottleRestartsSilently\n";

    {   // resqueeze during the min step
        ThrottleCalibrationLogic c;
        assert(feed(c, 900, 0, 3000) == CalibrationEvent::MaxCaptured && c.pinMax() == 900);
        assert(c.update(2400, 3010) == CalibrationEvent::None);
        assert(c.step() == 0);
        assert(feed(c, 2400, 3020, 6010) == CalibrationEvent::MaxCaptured && c.pinMax() == 2400);
        assert(feed(c, 900, 6020, 9020) == CalibrationEvent::Complete);
        assert(c.pinMin() == 900 && c.pinMax() == 2400);
    }
    cout << "PASS: resqueezeDuringMinStep\n";

    {   // partial squeeze
        ThrottleCalibrationLogic c;
        assert(feed(c, 1300, 0, 3000) == CalibrationEvent::MaxCaptured);
        assert(feed(c, 900, 3010, 6010) == CalibrationEvent::RestartWithBeep);
        assert(c.step() == 0 && !c.isCalibrated());
    }
    cout << "PASS: partialSqueezeBeeps\n";

    {   // unstable hold never completes
        ThrottleCalibrationLogic c;
        for (uint32_t t = 0; t <= 10000; t += 10) {
            int v = ((t / 10) % 2) ? 2500 : 2400;
            assert(c.update(v, t) != CalibrationEvent::MaxCaptured);
        }
        assert(c.step() == 0);
    }
    cout << "PASS: unstableHoldNeverCompletes\n";

    {   // hold value is the mean, not the last sample
        ThrottleCalibrationLogic c;
        CalibrationEvent e = CalibrationEvent::None;
        for (uint32_t i = 0; i <= 300; ++i) {
            int v = 2380 + (int)((i % 21) * 2);   // 2380..2420, mean 2400
            CalibrationEvent x = c.update(v, i * 10);
            if (x != CalibrationEvent::None) e = x;
        }
        assert(e == CalibrationEvent::MaxCaptured);
        assert(abs(c.pinMax() - 2400) <= 2);
    }
    cout << "PASS: holdMeanNotLastSample\n";

    normalProcedure(0xFFFFF000u);
    cout << "PASS: rollover\n";
    return 0;
}
