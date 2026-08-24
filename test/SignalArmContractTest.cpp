// test/SignalArmContractTest.cpp
#include <cassert>
#include <iostream>
#include "../src/Power/SignalArmContract.h"
using namespace std;

static const uint32_t GRACE = 2000;

// Arms the contract and settles the deferred snapshot with one update() at
// t=0, mirroring what Power::checkSignalLoss() does on the first tick after
// arming.
static void armWith(SignalArmContract& c, bool validAtArm) {
    c.onArmed();
    // The snapshot tick can never disarm.
    assert(c.update(validAtArm, 0, GRACE) == SignalArmContract::Outcome::None);
}

void test_valid_at_arm_limits_while_still_valid() {
    SignalArmContract c;
    armWith(c, true);
    assert(c.shouldLimit(true));
    cout << "PASS: valid at arm, still valid -> limiting applies\n";
}

void test_snapshot_is_deferred_to_first_update() {
    SignalArmContract c;
    c.onArmed();
    // Before the first update() there is no contract yet: nothing limits and
    // nothing can disarm, whatever the signal reads.
    assert(!c.shouldLimit(true));
    c.update(true, 0, GRACE);
    assert(c.shouldLimit(true));
    cout << "PASS: onArmed() defers the snapshot to the first update()\n";
}

void test_snapshot_tick_never_disarms_even_when_invalid() {
    SignalArmContract c;
    c.onArmed();
    // This is the arm-immediately-disarms bug: a signal reading invalid on
    // the very first tick after arming is "invalid at arm", never a loss.
    assert(c.update(false, 0, GRACE) == SignalArmContract::Outcome::None);
    assert(c.update(false, 100000, GRACE) == SignalArmContract::Outcome::None);
    cout << "PASS: invalid on the snapshot tick -> disabled, never an instant disarm\n";
}

void test_valid_at_arm_then_invalid_disarms_only_after_grace() {
    SignalArmContract c;
    armWith(c, true);
    assert(c.update(false, 1, GRACE) == SignalArmContract::Outcome::None);            // loss starts
    assert(c.update(false, GRACE - 1, GRACE) == SignalArmContract::Outcome::None);    // still inside the window
    assert(c.update(false, GRACE + 1, GRACE) == SignalArmContract::Outcome::LostInvalid);     // grace elapsed -> disarm
    cout << "PASS: valid at arm, invalid for the full grace window -> disarm\n";
}

void test_brief_invalid_flicker_does_not_disarm() {
    SignalArmContract c;
    armWith(c, true);
    // The CAN ESC_STATUS case: Stale for a few hundred ms, then fresh again.
    assert(c.update(false, 100, GRACE) == SignalArmContract::Outcome::None);
    assert(c.update(false, 900, GRACE) == SignalArmContract::Outcome::None);
    assert(c.update(true, 1000, GRACE) == SignalArmContract::Outcome::None);
    // A later loss must be timed from scratch, not from the first flicker.
    assert(c.update(false, 1100, GRACE) == SignalArmContract::Outcome::None);
    assert(c.update(false, 3000, GRACE) == SignalArmContract::Outcome::None);         // 1900ms in, still under
    assert(c.update(false, 3101, GRACE) == SignalArmContract::Outcome::LostInvalid);  // 2001ms in -> disarm
    cout << "PASS: a brief flicker resets the debounce instead of disarming\n";
}

void test_valid_at_arm_then_invalid_stops_limiting_immediately() {
    SignalArmContract c;
    armWith(c, true);
    // Limiting stops on the invalid sample itself — no grace. Grace only
    // delays the disarm; it must never let a bogus reading drive the limiter.
    assert(!c.shouldLimit(false));
    cout << "PASS: an invalid sample stops limiting immediately, regardless of grace\n";
}

void test_invalid_at_arm_never_limits_even_if_it_recovers() {
    SignalArmContract c;
    armWith(c, false);
    assert(!c.shouldLimit(true));
    cout << "PASS: invalid at arm -> disabled all session, even after recovering\n";
}

void test_invalid_at_arm_never_disarms() {
    SignalArmContract c;
    armWith(c, false);
    assert(c.update(false, 10000, GRACE) == SignalArmContract::Outcome::None);
    assert(c.update(true, 20000, GRACE) == SignalArmContract::Outcome::None);
    cout << "PASS: invalid at arm -> never triggers a disarm from this signal\n";
}

void test_still_valid_never_triggers_disarm() {
    SignalArmContract c;
    armWith(c, true);
    for (uint32_t t = 1; t < 100000; t += 5000) {
        assert(c.update(true, t, GRACE) == SignalArmContract::Outcome::None);
    }
    cout << "PASS: still valid -> no disarm\n";
}

void test_rearming_takes_a_fresh_snapshot() {
    SignalArmContract c;
    armWith(c, true);
    assert(c.update(false, 1, GRACE) == SignalArmContract::Outcome::None);        // loss starts
    assert(c.update(false, GRACE + 2, GRACE) == SignalArmContract::Outcome::LostInvalid); // grace elapsed, disarms

    c.onArmed();                               // pilot re-arms, sensor still bad
    assert(c.update(false, GRACE + 50, GRACE) == SignalArmContract::Outcome::None);
    assert(!c.shouldLimit(true));              // disabled all session now
    assert(c.update(false, 100000, GRACE) == SignalArmContract::Outcome::None);   // and never disarms again
    cout << "PASS: a fresh onArmed() call replaces the previous session's contract\n";
}

void test_rearming_clears_a_pending_loss_timer() {
    SignalArmContract c;
    armWith(c, true);
    assert(c.update(false, 1000, GRACE) == SignalArmContract::Outcome::None);     // loss in progress, not yet expired

    c.onArmed();                               // re-armed while it was pending
    assert(c.update(true, 1100, GRACE) == SignalArmContract::Outcome::None);      // snapshot: valid again
    // The stale loss timer from the previous session must not carry over and
    // disarm the moment the signal blips.
    assert(c.update(false, 1200, GRACE) == SignalArmContract::Outcome::None);
    assert(c.update(false, 3300, GRACE) == SignalArmContract::Outcome::LostInvalid);      // 2100ms of the NEW episode
    cout << "PASS: re-arming discards an in-progress loss timer\n";
}

void test_millis_rollover() {
    SignalArmContract c;
    const uint32_t nearMax = 0xFFFFFFFFu - 500;
    c.onArmed();
    c.update(true, nearMax, GRACE);
    assert(c.update(false, nearMax + 100, GRACE) == SignalArmContract::Outcome::None); // loss starts pre-rollover
    // Wraps past zero; unsigned subtraction must still measure the real gap.
    // Loss started at 0xFFFFFFFF-400, so elapsed at time t is t + 401.
    assert(c.update(false, 1000, GRACE) == SignalArmContract::Outcome::None);          // 1401ms elapsed
    assert(c.update(false, 1599, GRACE) == SignalArmContract::Outcome::LostInvalid);           // 2000ms elapsed
    cout << "PASS: the loss debounce survives millis() rollover\n";
}

void test_source_change_at_arm_snapshot() {
    SignalArmContract c;
    c.onArmed();
    // The snapshot tick captures validity AND the tag together.
    assert(c.update(true, 0, GRACE, 1) == SignalArmContract::Outcome::None);
    // The armed tag is now 1; tag 0 diverges, tag 1 is fine.
    assert(c.shouldLimit(true, 1));
    assert(!c.shouldLimit(true, 0));
    cout << "PASS: the snapshot tick captures the source tag alongside validity\n";
}

void test_tag_change_stays_changed_fires_source_changed_after_grace() {
    SignalArmContract c;
    armWith(c, true);                              // armed with tag 0
    assert(c.update(true, 1, GRACE, 1) == SignalArmContract::Outcome::None);       // divergence starts
    assert(c.update(true, GRACE - 1, GRACE, 1) == SignalArmContract::Outcome::None); // inside window
    assert(c.update(true, GRACE + 1, GRACE, 1) == SignalArmContract::Outcome::SourceChanged);
    cout << "PASS: a persistent source change fires SourceChanged only after grace\n";
}

void test_tag_flicker_back_resets_debounce() {
    SignalArmContract c;
    armWith(c, true);                              // armed with tag 0
    assert(c.update(true, 100, GRACE, 1) == SignalArmContract::Outcome::None);     // diverges
    assert(c.update(true, 900, GRACE, 0) == SignalArmContract::Outcome::None);     // back to armed tag
    // A later divergence is timed from scratch, not from the first one.
    assert(c.update(true, 1100, GRACE, 1) == SignalArmContract::Outcome::None);    // divergence 2 starts
    assert(c.update(true, 3000, GRACE, 1) == SignalArmContract::Outcome::None);    // 1900ms in
    assert(c.update(true, 3101, GRACE, 1) == SignalArmContract::Outcome::SourceChanged); // 2001ms in
    cout << "PASS: a source flicker back to the armed tag resets the debounce\n";
}

void test_tag_change_and_invalid_reading_fires_lost_invalid() {
    SignalArmContract c;
    armWith(c, true);                              // armed with tag 0
    // Tag changed AND the new reading is invalid (CAN lost, NTC unplugged):
    // precedence says "sensor failed", not "source changed".
    assert(c.update(false, 1, GRACE, 1) == SignalArmContract::Outcome::None);
    assert(c.update(false, GRACE + 1, GRACE, 1) == SignalArmContract::Outcome::LostInvalid);
    cout << "PASS: tag change + invalid reading -> LostInvalid (precedence rule)\n";
}

void test_invalid_at_arm_source_change_never_fires() {
    SignalArmContract c;
    armWith(c, false);                             // no sensor at all at arm
    // The incident case: CAN absent at arm, ESC starts Status 5 mid-flight.
    assert(c.update(true, 1000, GRACE, 1) == SignalArmContract::Outcome::None);
    assert(c.update(true, 20000, GRACE, 1) == SignalArmContract::Outcome::None);
    assert(!c.shouldLimit(true, 1));               // limiting stays disabled
    assert(!c.shouldLimit(true, 0));
    cout << "PASS: invalid at arm -> a later source appearing never disarms or limits\n";
}

void test_source_change_stops_limiting_immediately() {
    SignalArmContract c;
    armWith(c, true);                              // armed with tag 0
    // Goal 2: the limiter must never run one sensor's thresholds against
    // another sensor's reading — not even for one loop iteration. Limiting
    // stops the instant the tag diverges, before any disarm can fire.
    assert(!c.shouldLimit(true, 1));
    assert(c.update(true, 1, GRACE, 1) == SignalArmContract::Outcome::None);
    assert(!c.shouldLimit(true, 1));
    cout << "PASS: a diverging source stops limiting immediately, grace only delays the disarm\n";
}

void test_tag_omitted_matches_snapshot_zero() {
    SignalArmContract c;
    // Regression guard for ESC temp / battery voltage: they pass no tag, so
    // the snapshot is 0 and every later no-tag call compares 0 == 0.
    armWith(c, true);
    assert(c.shouldLimit(true));                   // no tag -> tag 0 == armed tag 0
    assert(c.update(true, 100, GRACE) == SignalArmContract::Outcome::None);
    assert(c.update(false, 100, GRACE) == SignalArmContract::Outcome::None);       // loss starts at t=100
    assert(c.update(false, GRACE, GRACE) == SignalArmContract::Outcome::None);     // 1900ms in, still under
    assert(c.update(false, GRACE + 101, GRACE) == SignalArmContract::Outcome::LostInvalid); // 2001ms in -> disarm
    cout << "PASS: omitting the tag behaves exactly as before (ESC temp / battery)\n";
}

int main() {
    test_valid_at_arm_limits_while_still_valid();
    test_snapshot_is_deferred_to_first_update();
    test_snapshot_tick_never_disarms_even_when_invalid();
    test_valid_at_arm_then_invalid_disarms_only_after_grace();
    test_brief_invalid_flicker_does_not_disarm();
    test_valid_at_arm_then_invalid_stops_limiting_immediately();
    test_invalid_at_arm_never_limits_even_if_it_recovers();
    test_invalid_at_arm_never_disarms();
    test_still_valid_never_triggers_disarm();
    test_rearming_takes_a_fresh_snapshot();
    test_rearming_clears_a_pending_loss_timer();
    test_millis_rollover();
    test_source_change_at_arm_snapshot();
    test_tag_change_stays_changed_fires_source_changed_after_grace();
    test_tag_flicker_back_resets_debounce();
    test_tag_change_and_invalid_reading_fires_lost_invalid();
    test_invalid_at_arm_source_change_never_fires();
    test_source_change_stops_limiting_immediately();
    test_tag_omitted_matches_snapshot_zero();
    cout << "SignalArmContractTest: all passed" << endl;
    return 0;
}
