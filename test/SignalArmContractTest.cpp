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
    bool disarm = c.update(validAtArm, 0, GRACE);
    assert(!disarm); // the snapshot tick can never disarm
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
    assert(!c.update(false, 0, GRACE));
    assert(!c.update(false, 100000, GRACE));
    cout << "PASS: invalid on the snapshot tick -> disabled, never an instant disarm\n";
}

void test_valid_at_arm_then_invalid_disarms_only_after_grace() {
    SignalArmContract c;
    armWith(c, true);
    assert(!c.update(false, 1, GRACE));            // loss starts
    assert(!c.update(false, GRACE - 1, GRACE));    // still inside the window
    assert(c.update(false, GRACE + 1, GRACE));     // grace elapsed -> disarm
    cout << "PASS: valid at arm, invalid for the full grace window -> disarm\n";
}

void test_brief_invalid_flicker_does_not_disarm() {
    SignalArmContract c;
    armWith(c, true);
    // The CAN ESC_STATUS case: Stale for a few hundred ms, then fresh again.
    assert(!c.update(false, 100, GRACE));
    assert(!c.update(false, 900, GRACE));
    assert(!c.update(true, 1000, GRACE));
    // A later loss must be timed from scratch, not from the first flicker.
    assert(!c.update(false, 1100, GRACE));
    assert(!c.update(false, 3000, GRACE));         // 1900ms in, still under
    assert(c.update(false, 3101, GRACE));          // 2001ms in -> disarm
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
    assert(!c.update(false, 10000, GRACE));
    assert(!c.update(true, 20000, GRACE));
    cout << "PASS: invalid at arm -> never triggers a disarm from this signal\n";
}

void test_still_valid_never_triggers_disarm() {
    SignalArmContract c;
    armWith(c, true);
    for (uint32_t t = 1; t < 100000; t += 5000) {
        assert(!c.update(true, t, GRACE));
    }
    cout << "PASS: still valid -> no disarm\n";
}

void test_rearming_takes_a_fresh_snapshot() {
    SignalArmContract c;
    armWith(c, true);
    assert(!c.update(false, 1, GRACE));        // loss starts
    assert(c.update(false, GRACE + 2, GRACE)); // grace elapsed, disarms

    c.onArmed();                               // pilot re-arms, sensor still bad
    assert(!c.update(false, GRACE + 50, GRACE));
    assert(!c.shouldLimit(true));              // disabled all session now
    assert(!c.update(false, 100000, GRACE));   // and never disarms again
    cout << "PASS: a fresh onArmed() call replaces the previous session's contract\n";
}

void test_rearming_clears_a_pending_loss_timer() {
    SignalArmContract c;
    armWith(c, true);
    assert(!c.update(false, 1000, GRACE));     // loss in progress, not yet expired

    c.onArmed();                               // re-armed while it was pending
    assert(!c.update(true, 1100, GRACE));      // snapshot: valid again
    // The stale loss timer from the previous session must not carry over and
    // disarm the moment the signal blips.
    assert(!c.update(false, 1200, GRACE));
    assert(c.update(false, 3300, GRACE));      // 2100ms of the NEW episode
    cout << "PASS: re-arming discards an in-progress loss timer\n";
}

void test_millis_rollover() {
    SignalArmContract c;
    const uint32_t nearMax = 0xFFFFFFFFu - 500;
    c.onArmed();
    c.update(true, nearMax, GRACE);
    assert(!c.update(false, nearMax + 100, GRACE)); // loss starts pre-rollover
    // Wraps past zero; unsigned subtraction must still measure the real gap.
    // Loss started at 0xFFFFFFFF-400, so elapsed at time t is t + 401.
    assert(!c.update(false, 1000, GRACE));          // 1401ms elapsed
    assert(c.update(false, 1599, GRACE));           // 2000ms elapsed
    cout << "PASS: the loss debounce survives millis() rollover\n";
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
    cout << "SignalArmContractTest: all passed" << endl;
    return 0;
}
