#include <iostream>
#include <cassert>
#include <stdint.h>
#include <stdbool.h>
using namespace std;

#include "../src/Sound/SoundLogic.h"

void test_continuous_state_never_expires() {
    SoundLogic logic;
    logic.setStatePattern(SoundState::ArmedIdle, {2000, 200, 200, 0});
    logic.setState(SoundState::ArmedIdle);

    bool expectedOn = true;
    for (int i = 0; i < 2000; i++) {
        // 2000 half-cycles * 200ms = 400,000ms (~6.6 min) simulated, far past
        // the old 255-rep / ~102s cutoff that used to silence this alert.
        uint32_t t = (uint32_t)i * 200;
        SoundOutput o = logic.update(t);
        assert(o.toneOn == expectedOn);
        assert(o.freqHz == 2000);
        expectedOn = !expectedOn;
    }
    cout << "PASS: continuous state alternates for 2000 half-cycles, never expires\n";
}

void test_event_preempts_state_and_resumes_from_start() {
    SoundLogic logic;
    logic.setStatePattern(SoundState::ArmedIdle, {2000, 200, 200, 0});
    logic.setEventPattern(SoundEvent::ButtonClick, {3000, 50, 0, 1});

    logic.setState(SoundState::ArmedIdle);
    SoundOutput o = logic.update(0);
    assert(o.toneOn && o.freqHz == 2000);

    // Halfway through the state's on-phase, an event preempts it immediately.
    logic.pushEvent(SoundEvent::ButtonClick);
    o = logic.update(50);
    assert(o.toneOn && o.freqHz == 3000);
    assert(!logic.stateActive()); // cut, not paused-in-place

    o = logic.update(90); // elapsed 40 < 50(onMs), still playing
    assert(o.toneOn && o.freqHz == 3000);

    o = logic.update(100); // elapsed 50 >= 50(onMs), reps=1 -> finishes, hands off to state
    assert(o.toneOn && o.freqHz == 2000);
    assert(logic.stateActive());

    // "Resumes from the start of its cycle": a fresh 200ms on-phase from
    // t=100, not a continuation of the original cycle (which started at t=0
    // and would already be in its off-phase by t=299). Pick timings so the
    // two hypotheses disagree.
    o = logic.update(100 + 199);
    assert(o.toneOn); // still on 199ms after the resume point
    o = logic.update(100 + 200);
    assert(!o.toneOn); // flips off exactly 200ms after the RESUME point (t=300)

    cout << "PASS: event preempts state immediately and state resumes from cycle start\n";
}

void test_setstate_idempotent_and_none_silences_immediately() {
    SoundLogic logic;
    logic.setStatePattern(SoundState::ArmedIdle, {2000, 200, 200, 0});

    logic.setState(SoundState::ArmedIdle);
    assert(logic.update(0).toneOn == true);

    // Calling setState() with the same value every tick must not restart
    // the on-phase.
    for (uint32_t t = 1; t <= 199; t++) {
        logic.setState(SoundState::ArmedIdle);
        assert(logic.update(t).toneOn == true);
    }
    logic.setState(SoundState::ArmedIdle);
    assert(logic.update(200).toneOn == false); // flips exactly at 200, unaffected by 200 redundant calls

    // setState(None) silences on the very next tick, without waiting for
    // the current phase to finish.
    logic.setState(SoundState::None);
    assert(logic.update(201).toneOn == false);
    assert(!logic.stateActive());

    cout << "PASS: setState is idempotent; None silences immediately\n";
}

void test_four_queued_events_play_in_order_no_gap() {
    SoundLogic logic;
    logic.setEventPattern(SoundEvent::SystemStart,     {1000, 10, 0, 1});
    logic.setEventPattern(SoundEvent::CalibrationStep, {2000, 20, 0, 1});
    logic.setEventPattern(SoundEvent::ButtonClick,     {3000, 5,  0, 1});
    logic.setEventPattern(SoundEvent::Disarmed,        {4000, 15, 0, 1});

    logic.pushEvent(SoundEvent::SystemStart);
    logic.pushEvent(SoundEvent::CalibrationStep);
    logic.pushEvent(SoundEvent::ButtonClick);
    logic.pushEvent(SoundEvent::Disarmed);

    for (uint32_t t = 0; t <= 49; t++) {
        SoundOutput o = logic.update(t);
        assert(o.toneOn);
        uint16_t expectedFreq =
            (t < 10) ? 1000 :
            (t < 30) ? 2000 :
            (t < 35) ? 3000 : 4000;
        assert(o.freqHz == expectedFreq);
    }
    SoundOutput last = logic.update(50);
    assert(!last.toneOn); // queue drained, no state configured -> silence
    assert(logic.currentEvent() == SoundEvent::kCount);
    assert(logic.queueLength() == 0);
    cout << "PASS: four queued events play back-to-back in order with no gap\n";
}

void test_queue_full_drops_newest_preserves_inflight() {
    SoundLogic logic;
    logic.setEventPattern(SoundEvent::SystemStart,     {1000, 10, 0, 1});
    logic.setEventPattern(SoundEvent::CalibrationStep, {2000, 20, 0, 1});
    logic.setEventPattern(SoundEvent::ButtonClick,     {3000, 5,  0, 1});
    logic.setEventPattern(SoundEvent::Disarmed,        {4000, 15, 0, 1});
    logic.setEventPattern(SoundEvent::LinkLoss,        {9999, 999, 0, 1}); // must never play

    logic.pushEvent(SoundEvent::SystemStart);
    logic.pushEvent(SoundEvent::CalibrationStep);
    logic.pushEvent(SoundEvent::ButtonClick);
    logic.pushEvent(SoundEvent::Disarmed);
    assert(logic.queueLength() == 4);

    logic.pushEvent(SoundEvent::LinkLoss); // queue full -> dropped
    assert(logic.droppedEvents() == 1);
    assert(logic.queueLength() == 4); // unchanged

    bool sawLinkLossFreq = false;
    for (uint32_t t = 0; t <= 60; t++) {
        SoundOutput o = logic.update(t);
        if (o.toneOn && o.freqHz == 9999) sawLinkLossFreq = true;
    }
    assert(!sawLinkLossFreq);
    assert(logic.currentEvent() == SoundEvent::kCount);
    assert(logic.queueLength() == 0);
    cout << "PASS: queue full drops the newest event; in-flight sequence plays intact\n";
}

void test_millis_rollover_state_and_event() {
    {
        SoundLogic logic;
        logic.setStatePattern(SoundState::ArmedIdle, {2000, 200, 200, 0});
        logic.setState(SoundState::ArmedIdle);

        uint32_t nearMax = 0xFFFFFFF0u; // 16ms before wraparound
        assert(logic.update(nearMax).toneOn == true);

        uint32_t justAfterWrap = 20u;   // real elapsed since phase start: 16 + 20 = 36ms
        assert(logic.update(justAfterWrap).toneOn == true); // 36 < 200(onMs)

        uint32_t pastBoundary = 200u;   // real elapsed: 16 + 200 = 216ms >= 200(onMs)
        assert(logic.update(pastBoundary).toneOn == false);
    }
    {
        SoundLogic logic;
        logic.setEventPattern(SoundEvent::ButtonClick, {3000, 50, 0, 1});
        logic.pushEvent(SoundEvent::ButtonClick);

        uint32_t nearMax = 0xFFFFFFE0u; // 32ms before wraparound
        assert(logic.update(nearMax).toneOn == true);

        uint32_t justAfterWrap = 10u;   // real elapsed: 32 + 10 = 42ms
        assert(logic.update(justAfterWrap).toneOn == true); // 42 < 50(onMs)

        uint32_t pastBoundary = 20u;    // real elapsed: 32 + 20 = 52ms >= 50(onMs)
        SoundOutput o = logic.update(pastBoundary);
        assert(o.toneOn == false);
        assert(logic.currentEvent() == SoundEvent::kCount);
    }
    cout << "PASS: millis() rollover handled correctly for both state and event phases\n";
}

void test_tone_transition() {
    assert(computeToneTransition(false, 0, false, 0) == ToneTransition::None);
    assert(computeToneTransition(true, 2000, false, 0) == ToneTransition::TurnOff);
    assert(computeToneTransition(false, 0, true, 2000) == ToneTransition::TurnOn);
    assert(computeToneTransition(true, 2000, true, 2000) == ToneTransition::None);
    assert(computeToneTransition(true, 2000, true, 3000) == ToneTransition::Retune);
    cout << "PASS: computeToneTransition covers all cases; a frequency change while on is Retune\n";
}

int main() {
    test_continuous_state_never_expires();
    test_event_preempts_state_and_resumes_from_start();
    test_setstate_idempotent_and_none_silences_immediately();
    test_four_queued_events_play_in_order_no_gap();
    test_queue_full_drops_newest_preserves_inflight();
    test_millis_rollover_state_and_event();
    test_tone_transition();
    return 0;
}
