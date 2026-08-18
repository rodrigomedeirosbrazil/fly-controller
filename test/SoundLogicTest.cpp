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

// Regression: a long-running *event* cannot be cancelled and calls
// stateRunner_.stop() on every tick it runs, so a fault-disarm alarm modelled
// as an event kept sounding through a successful re-arm and queued every
// other beep behind itself. These two tests pin down why the fault alarm has
// to live on the state layer.
void test_a_long_event_cannot_be_cancelled_or_yield_to_a_state() {
    SoundLogic logic;
    logic.setEventPattern(SoundEvent::PowerAlert, {2500, 80, 300, 255}); // ~97s
    logic.setStatePattern(SoundState::ArmedIdle,  {2000, 200, 200, 0});

    logic.pushEvent(SoundEvent::PowerAlert);
    logic.update(0);

    // Declaring a state while the long event runs achieves nothing: the event
    // owns the output and re-stops the state runner every tick.
    logic.setState(SoundState::ArmedIdle);
    for (uint32_t t = 100; t <= 30000; t += 100) {
        logic.update(t);
        assert(!logic.stateActive());
    }
    assert(logic.currentEvent() == SoundEvent::PowerAlert);

    // Even setState(None) can't silence it -- there is no cancel path.
    logic.setState(SoundState::None);
    logic.update(30100);
    assert(logic.currentEvent() == SoundEvent::PowerAlert);

    cout << "PASS: a long event is uncancellable and starves the state layer\n";
}

void test_fault_disarm_state_stops_the_moment_the_condition_clears() {
    SoundLogic logic;
    logic.setStatePattern(SoundState::FaultDisarm, {2500, 80,  300, 0});
    logic.setStatePattern(SoundState::ArmedIdle,   {2000, 200, 200, 0});

    // Fault disarm: alarm sounds, and keeps sounding (reps=0, no expiry).
    logic.setState(SoundState::FaultDisarm);
    assert(logic.update(0).toneOn == true);
    for (uint32_t t = 1000; t <= 200000; t += 1000) {
        logic.update(t);
        assert(logic.currentStateId() == SoundState::FaultDisarm);
    }

    // Pilot re-arms: the condition clears, and the alarm must hand straight
    // over to the armed-idle alert -- this is what the event version could
    // not do.
    logic.setState(SoundState::ArmedIdle);
    SoundOutput out = logic.update(200001);
    assert(logic.currentStateId() == SoundState::ArmedIdle);
    assert(out.freqHz == 2000);
    assert(out.toneOn == true);

    cout << "PASS: the fault-disarm state yields immediately on re-arm\n";
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

void test_setstatefreq_applies_at_on_off_edge() {
    SoundLogic logic;
    logic.setStatePattern(SoundState::ArmCharging, {1800, 60, 40, 0});
    logic.setState(SoundState::ArmCharging);
    logic.setStateFreq(2000);

    // First on-phase uses the pattern default; a live retune mid-tone is not
    // applied (that would be an audible click on the piezo).
    SoundOutput o = logic.update(0);
    assert(o.toneOn && o.freqHz == 1800);
    o = logic.update(30);
    assert(o.toneOn && o.freqHz == 1800);

    // on->off edge at t=60: the pending frequency lands for the next on-phase.
    o = logic.update(60);
    assert(!o.toneOn);
    o = logic.update(100);
    assert(o.toneOn && o.freqHz == 2000);

    // A retune requested while on is deferred to the next edge.
    logic.setStateFreq(2400);
    o = logic.update(120);
    assert(o.toneOn && o.freqHz == 2000);
    cout << "PASS: setStateFreq is applied only at the on->off phase edge\n";
}

void test_setstatefreq_does_not_leak_across_states() {
    SoundLogic logic;
    logic.setStatePattern(SoundState::ArmCharging,   {1800, 60,  40,  0});
    logic.setStatePattern(SoundState::ArmedIdle,     {2000, 200, 200, 0});
    logic.setState(SoundState::ArmCharging);
    logic.setStateFreq(2300);
    logic.update(0);   // on-phase of ArmCharging starts; pending 2300 unconsumed

    // Transition before the pending frequency was applied.
    logic.setState(SoundState::ArmedIdle);
    logic.update(1);
    SoundOutput o = logic.update(201); // ArmedIdle's own on->off edge
    assert(o.freqHz == 2000);          // default, not the stale 2300
    cout << "PASS: a stale setStateFreq never leaks into another state\n";
}

void test_setstatefreq_does_not_persist_across_sessions() {
    // Regression: the retune used to mutate the catalog pattern, so after one
    // disarm ramp DisarmRamping was stuck at the bottom of the sweep and the
    // next disarm started low then jumped up — inverting the descending cue.
    SoundLogic logic;
    logic.setStatePattern(SoundState::DisarmRamping, {2500, 60, 40, 0});
    logic.setState(SoundState::DisarmRamping);
    logic.setStateFreq(1800);   // pilot holds until near powerScale 0
    logic.update(0);            // on, 2500
    logic.update(60);           // on->off -> runtime freq 1800
    assert(logic.update(100).freqHz == 1800);

    // Next session must start from the catalog default and sweep again.
    logic.setState(SoundState::None);
    logic.update(200);
    logic.setState(SoundState::DisarmRamping);
    logic.update(300);          // start -> freq 2500 (default)
    SoundOutput o = logic.update(360); // first on->off edge of the new session
    assert(o.freqHz == 2500);          // default, not the swept 1800
    cout << "PASS: a retune never persists into a later session\n";
}

int main() {
    test_continuous_state_never_expires();
    test_event_preempts_state_and_resumes_from_start();
    test_setstate_idempotent_and_none_silences_immediately();
    test_a_long_event_cannot_be_cancelled_or_yield_to_a_state();
    test_fault_disarm_state_stops_the_moment_the_condition_clears();
    test_four_queued_events_play_in_order_no_gap();
    test_queue_full_drops_newest_preserves_inflight();
    test_millis_rollover_state_and_event();
    test_tone_transition();
    test_setstatefreq_applies_at_on_off_edge();
    test_setstatefreq_does_not_leak_across_states();
    test_setstatefreq_does_not_persist_across_sessions();
    return 0;
}
