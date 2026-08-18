// test/ButtonGestureLogicTest.cpp
#include <cassert>
#include <iostream>
#include <stdint.h>
#include "../src/Button/ButtonGestureLogic.h"
using namespace std;

// Timestamps are debounced-edge-relative: each raw level change is held >= the
// 20 ms debounce before the edge commits. The integration anchor is reset on
// every edge, so values are exact as long as update() is ticked at the anchor
// and at the measurement point.

void test_parity_arm_gesture() {
    ButtonGestureLogic logic;
    const bool engaged = true;

    // Short click: 100 ms press + release.
    ButtonGestureOutput o = logic.update(0, true, false, engaged);    // raw press
    o = logic.update(20, true, false, engaged);                       // press edge
    o = logic.update(100, false, false, engaged);                     // raw release
    o = logic.update(120, false, false, engaged);                     // release edge
    assert(o.intent == ButtonIntent::Click);
    assert(o.armCharge == 0);

    // Hold 2000 ms starting inside the 600 ms window.
    o = logic.update(200, true, false, engaged);                      // raw press
    o = logic.update(220, true, false, engaged);                      // press edge @220
    assert(o.intent == ButtonIntent::None);
    o = logic.update(2220, true, false, engaged);                     // 2000 ms of hold
    assert(o.intent == ButtonIntent::Arm);
    cout << "PASS: click + hold within window arms\n";

    // Hold started outside the window (after it expired) does nothing.
    ButtonGestureLogic logic2;
    logic2.update(0, true, false, engaged);
    logic2.update(20, true, false, engaged);
    logic2.update(100, false, false, engaged);
    logic2.update(120, false, false, engaged);                        // click, window to t=3620
    logic2.update(4000, false, false, engaged);                       // window expired
    logic2.update(4100, true, false, engaged);                        // raw press from Idle
    o = logic2.update(4120, true, false, engaged);                    // press edge
    o = logic2.update(6120, true, false, engaged);                    // held 2000 ms -> nothing
    assert(o.intent == ButtonIntent::None);
    o = logic2.update(6200, false, false, engaged);
    o = logic2.update(6220, false, false, engaged);                   // release, not a click
    assert(o.intent == ButtonIntent::None);
    cout << "PASS: hold started outside the window never arms\n";
}

void test_arm_window_boundary() {
    // The window runs from the release edge of the click to the press edge of
    // the hold. Expiry is checked before the press edge in the same tick, so a
    // press landing exactly at the boundary is already too late.
    const bool engaged = true;
    const uint32_t clickReleaseAt = 120;
    const uint32_t windowCloses   = clickReleaseAt + ButtonGestureLogic::BUTTON_ARM_WINDOW_MS;

    // Inside: press edge commits one debounce tick (20 ms) before the window
    // closes -> arms.
    ButtonGestureLogic inside;
    inside.update(0,   true,  false, engaged);
    inside.update(20,  true,  false, engaged);                        // press edge
    inside.update(100, false, false, engaged);
    ButtonGestureOutput o = inside.update(clickReleaseAt, false, false, engaged); // release edge -> Click
    assert(o.intent == ButtonIntent::Click);
    inside.update(windowCloses - 40, true, false, engaged);           // raw press
    inside.update(windowCloses - 20, true, false, engaged);           // press edge, just inside
    o = inside.update(windowCloses - 20 + ButtonGestureLogic::BUTTON_ARM_CHARGE_MS, true, false, engaged);
    assert(o.intent == ButtonIntent::Arm);

    // Outside: press edge commits exactly at the boundary tick, which the
    // expiry check catches first -> never arms.
    ButtonGestureLogic outside;
    outside.update(0,   true,  false, engaged);
    outside.update(20,  true,  false, engaged);
    outside.update(100, false, false, engaged);
    o = outside.update(clickReleaseAt, false, false, engaged);        // release edge -> Click
    assert(o.intent == ButtonIntent::Click);
    outside.update(windowCloses - 20, true, false, engaged);          // raw press
    outside.update(windowCloses,      true, false, engaged);          // press edge, just outside
    const uint32_t heldCheckAt = windowCloses + ButtonGestureLogic::BUTTON_ARM_CHARGE_MS;
    o = outside.update(heldCheckAt, true, false, engaged);
    assert(o.intent == ButtonIntent::None);                           // never arms
    o = outside.update(heldCheckAt + 100, false, false, engaged);
    o = outside.update(heldCheckAt + 120, false, false, engaged);     // release, not a click
    assert(o.intent == ButtonIntent::None);
    cout << "PASS: the arming window closes at BUTTON_ARM_WINDOW_MS\n";
}

void test_aborted_hold_denies_immediate_rearm() {
    // An abort resets the whole gesture to Idle, so a second press-and-hold
    // right after does not even start charging: there is no window left to be
    // inside of, and armCharge cannot sum across separate attempts because the
    // second attempt never begins.
    ButtonGestureLogic logic;
    const bool engaged = true;

    logic.update(0, true, false, engaged);
    logic.update(20, true, false, engaged);
    logic.update(100, false, false, engaged);
    logic.update(120, false, false, engaged);                         // click, window open (closes @720)

    // First partial hold: 400 ms, then release -> abort, back to Idle.
    logic.update(140, true, false, engaged);
    logic.update(160, true, false, engaged);                          // charge starts
    ButtonGestureOutput o = logic.update(560, true, false, engaged);
    assert(o.armCharge == 20);                                        // 400/2000 ms charged
    o = logic.update(580, false, false, engaged);                     // raw release
    o = logic.update(600, false, false, engaged);                     // release edge -> abort
    assert(o.armCharge == 0);                                         // resets on release

    // Second partial hold: the abort closed the window, so it never charges at
    // all — partial holds cannot sum, and they cannot even restart.
    logic.update(620, true, false, engaged);
    logic.update(640, true, false, engaged);
    o = logic.update(1040, true, false, engaged);
    assert(o.armCharge == 0);
    assert(o.intent != ButtonIntent::Arm);
    cout << "PASS: releasing the arm charge resets it; partial holds do not sum\n";
}

void test_aborted_hold_requires_new_click() {
    // Releasing before the charge completes throws the whole gesture away.
    // Re-arming needs a fresh first click, not just another hold.
    ButtonGestureLogic logic;
    const bool engaged = true;

    logic.update(0,   true,  false, engaged);
    logic.update(20,  true,  false, engaged);
    logic.update(100, false, false, engaged);
    ButtonGestureOutput o = logic.update(120, false, false, engaged);  // Click, window open
    assert(o.intent == ButtonIntent::Click);

    logic.update(200, true, false, engaged);
    logic.update(220, true, false, engaged);                           // charge starts
    o = logic.update(1220, true, false, engaged);
    assert(o.armCharge == 50);                                         // half charged
    logic.update(1240, false, false, engaged);                         // raw release
    o = logic.update(1260, false, false, engaged);                     // release edge -> abort
    assert(o.intent == ButtonIntent::None);                            // not a click
    assert(o.armCharge == 0);

    // A full 2000 ms hold right after the abort must NOT arm: no click, no window.
    logic.update(1300, true, false, engaged);
    logic.update(1320, true, false, engaged);
    o = logic.update(3320, true, false, engaged);
    assert(o.intent == ButtonIntent::None);
    assert(o.armCharge == 0);                                          // never even charges
    logic.update(3400, false, false, engaged);
    o = logic.update(3420, false, false, engaged);                     // release, not a click
    assert(o.intent == ButtonIntent::None);

    // A fresh click + hold arms normally.
    logic.update(3500, true,  false, engaged);
    logic.update(3520, true,  false, engaged);
    logic.update(3600, false, false, engaged);
    o = logic.update(3620, false, false, engaged);                     // Click, window open
    assert(o.intent == ButtonIntent::Click);
    logic.update(3700, true, false, engaged);
    logic.update(3720, true, false, engaged);                          // charge starts
    o = logic.update(5720, true, false, engaged);
    assert(o.intent == ButtonIntent::Arm);
    cout << "PASS: an aborted hold resets the gesture; re-arming needs a new click\n";
}

void test_short_abort_does_not_reopen_window() {
    // A sub-300 ms release out of ArmCharging used to count as a brand new
    // click and rewrite the window deadline, so chained taps kept the window
    // alive forever. It must now be swallowed entirely.
    ButtonGestureLogic logic;
    const bool engaged = true;

    logic.update(0,   true,  false, engaged);
    logic.update(20,  true,  false, engaged);
    logic.update(100, false, false, engaged);
    ButtonGestureOutput o = logic.update(120, false, false, engaged);  // Click, window open
    assert(o.intent == ButtonIntent::Click);

    logic.update(200, true, false, engaged);
    logic.update(220, true, false, engaged);                           // charge starts
    logic.update(300, false, false, engaged);                          // raw release @80 ms held
    o = logic.update(320, false, false, engaged);                      // release edge
    assert(o.intent == ButtonIntent::None);                            // swallowed, no beep

    // The window is gone: a hold started well within the old deadline does nothing.
    logic.update(400, true, false, engaged);
    logic.update(420, true, false, engaged);
    o = logic.update(2420, true, false, engaged);
    assert(o.intent == ButtonIntent::None);
    assert(o.armCharge == 0);
    cout << "PASS: a short abort does not re-open the arming window\n";
}

void test_disarm_ramp_timing() {
    ButtonGestureLogic logic;
    const bool engaged = true;
    logic.update(0, false, false, engaged);
    logic.update(5, false, true, engaged);                            // armed

    logic.update(20, true, true, engaged);                            // raw press
    logic.update(40, true, true, engaged);                            // press edge @40
    ButtonGestureOutput o = logic.update(1040, true, true, engaged);  // 1000 ms in
    assert(o.powerScale == 50);
    assert(o.intent == ButtonIntent::None);

    o = logic.update(2040, true, true, engaged);                      // 2000 ms in
    assert(o.intent == ButtonIntent::Disarm);
    assert(o.powerScale == 0);
    cout << "PASS: ramp is 50 at 1000 ms, 0 and Disarm at 2000 ms\n";
}

void test_disarm_ramp_recovery_and_inversion() {
    ButtonGestureLogic logic;
    const bool engaged = true;
    logic.update(0, false, false, engaged);
    logic.update(1, false, true, engaged);                            // armed

    logic.update(10, true, true, engaged);                            // raw press
    logic.update(30, true, true, engaged);                            // press edge @30
    logic.update(1230, false, true, engaged);                         // raw release
    ButtonGestureOutput o = logic.update(1250, false, true, engaged); // release edge
    assert(o.powerScale == 40);                                       // 1200 ms of pressed time

    o = logic.update(2450, false, true, engaged);                     // 1200 ms of recovery
    assert(o.powerScale == 100);                                      // symmetric: same span up
    cout << "PASS: release at 1200 ms rises from 40 and reaches 100 in 1200 ms\n";

    // Press mid-recovery: the rate inverts from the current value.
    o = logic.update(2600, false, true, engaged);                     // still 100, no drift
    assert(o.powerScale == 100);
    o = logic.update(3200, true, true, engaged);                      // raw press
    logic.update(3220, true, true, engaged);                          // press edge @3220
    o = logic.update(3420, true, true, engaged);                      // 200 ms later: fell 10
    assert(o.powerScale == 90);
    cout << "PASS: pressing mid-recovery inverts the ramp from the current value\n";
}

void test_no_power_disarms_immediately() {
    ButtonGestureLogic logic;
    logic.update(0, false, false, false);                             // not engaged
    logic.update(5, false, true, false);                              // armed, not engaged
    ButtonGestureOutput o = logic.update(20, true, true, false);      // raw press
    o = logic.update(40, true, true, false);                          // press edge
    assert(o.intent == ButtonIntent::Disarm);
    assert(o.powerScale == 100);                                      // never leaves 100
    cout << "PASS: un-engaged press disarms immediately, no ramp\n";
}

void test_engagement_drop_mid_ramp_disarms() {
    ButtonGestureLogic logic;
    const bool engaged = true;
    logic.update(0, false, false, engaged);
    logic.update(5, false, true, engaged);                            // armed
    logic.update(20, true, true, engaged);
    logic.update(40, true, true, engaged);                            // press edge
    ButtonGestureOutput o = logic.update(1040, true, true, engaged);  // mid-ramp
    assert(o.powerScale == 50);

    o = logic.update(1500, true, true, false);                        // throttle closed mid-ramp
    assert(o.intent == ButtonIntent::Disarm);
    assert(o.powerScale == 100);
    cout << "PASS: engagement drop mid-ramp disarms immediately\n";
}

void test_release_after_disarm_and_stuck_button() {
    ButtonGestureLogic logic;
    const bool engaged = true;
    logic.update(0, false, false, engaged);
    logic.update(1, false, true, engaged);                            // armed
    logic.update(10, true, true, engaged);                            // raw press
    logic.update(30, true, true, engaged);                            // press edge @30
    ButtonGestureOutput o = logic.update(2030, true, true, engaged);  // ramp to 0
    assert(o.intent == ButtonIntent::Disarm);

    // Letting go after the latch must not open ClickPending (no fresh press).
    logic.update(2040, true, false, engaged);                         // disarmed, still held
    o = logic.update(4000, false, false, engaged);                    // raw release
    o = logic.update(4020, false, false, engaged);                    // release edge
    assert(o.intent != ButtonIntent::Click);

    // A fresh short tap afterwards works normally.
    logic.update(4100, true, false, engaged);
    logic.update(4120, true, false, engaged);
    logic.update(4200, false, false, engaged);
    o = logic.update(4220, false, false, engaged);
    assert(o.intent == ButtonIntent::Click);
    cout << "PASS: release after a disarm does not re-open the arming window\n";

    // A button that sticks down while armed ramps to zero, disarms, and stays
    // disarmed — it can never re-arm because a stuck press never produces a
    // fresh click.
    ButtonGestureLogic stuck;
    stuck.update(0, false, false, engaged);
    stuck.update(1, false, true, engaged);                          // armed
    stuck.update(1000, true, true, engaged);                        // button sticks down
    stuck.update(1020, true, true, engaged);                        // press edge -> ramp starts
    o = stuck.update(3020, true, true, engaged);                    // 2000 ms ramp -> disarm
    assert(o.intent == ButtonIntent::Disarm);
    o = stuck.update(60000, true, false, engaged);                  // still held, disarmed
    assert(o.intent == ButtonIntent::None);
    cout << "PASS: a permanently-held button ramps down, disarms, and never re-arms\n";

    // Arming while the button is held must not immediately start a disarm
    // ramp: the press that armed is consumed for the duration of that hold.
    ButtonGestureLogic holdArm;
    holdArm.update(0, false, false, engaged);
    holdArm.update(5, true, false, engaged);                        // raw press
    holdArm.update(25, true, false, engaged);                       // press edge
    holdArm.update(2025, true, true, engaged);                      // armed while held
    o = holdArm.update(4025, true, true, engaged);                  // 2 s later, still held
    assert(o.intent == ButtonIntent::None);
    assert(o.powerScale == 100);
    cout << "PASS: arming while held consumes the press (no instant disarm)\n";
}

void test_external_disarm_mid_ramp_resets_scale() {
    ButtonGestureLogic logic;
    const bool engaged = true;
    logic.update(0, false, false, engaged);
    logic.update(5, false, true, engaged);                            // armed
    logic.update(20, true, true, engaged);
    logic.update(40, true, true, engaged);                            // press edge
    ButtonGestureOutput o = logic.update(1040, true, true, engaged);  // mid-ramp
    assert(o.powerScale == 50);

    // Fault disarm (e.g. signal loss) while the ramp is in progress.
    o = logic.update(2000, true, false, engaged);
    assert(o.powerScale == 100);                                      // reset
    assert(o.intent == ButtonIntent::None);
    cout << "PASS: armed going false externally resets powerScale to 100\n";
}

void test_debounce_suppresses_short_pulses() {
    ButtonGestureLogic logic;
    const bool engaged = true;

    // A 15 ms press pulse never commits an edge -> no intent.
    ButtonGestureOutput o = logic.update(0, true, false, engaged);
    o = logic.update(10, true, false, engaged);
    o = logic.update(15, false, false, engaged);                      // released before 20 ms
    o = logic.update(100, false, false, engaged);
    assert(o.intent == ButtonIntent::None);

    // A real tap afterwards still works.
    logic.update(200, true, false, engaged);
    logic.update(220, true, false, engaged);
    logic.update(300, false, false, engaged);
    o = logic.update(320, false, false, engaged);
    assert(o.intent == ButtonIntent::Click);
    cout << "PASS: pulses shorter than the debounce produce no intent\n";
}

void test_ramp_progresses_with_small_ticks() {
    // Regression: the ramp used a whole-percent per-tick accumulator. With the
    // real loop iterating every 2-10 ms (RawCommand needs >=400 Hz), each tick
    // truncates to 0 and the button could never disarm. The integrator now runs
    // in 0.01% fixed point, so a 2 ms loop still disarms on schedule.
    ButtonGestureLogic logic;
    const bool engaged = true;
    logic.update(0, false, false, engaged);
    logic.update(1, false, true, engaged);    // armed
    logic.update(2, true, true, engaged);     // raw press

    uint8_t scaleAtHalf = 0;
    uint32_t disarmAtMs = 0;
    for (uint32_t t = 4; t <= 2600; t += 2) {
        ButtonGestureOutput o = logic.update(t, true, true, engaged);
        if (t == 1022) scaleAtHalf = o.powerScale; // 1000 ms after the press edge
        if (o.intent == ButtonIntent::Disarm) {
            disarmAtMs = t;
            break;
        }
    }
    assert(scaleAtHalf == 50);
    assert(disarmAtMs >= 2020 && disarmAtMs <= 2030);
    cout << "PASS: a 2 ms loop ramps down to disarm on schedule (no truncation freeze)\n";
}

void test_engagement_drop_during_recovery_does_not_disarm() {
    // Regression: an engagement drop during recovery (button already released)
    // used to count as an immediate disarm. The pilot taps, lets go, and
    // closing the throttle is a normal reaction — the ramp exists precisely to
    // make this recoverable, so it must not disarm.
    ButtonGestureLogic logic;
    const bool engaged = true;
    logic.update(0, false, false, engaged);
    logic.update(1, false, true, engaged);      // armed
    logic.update(10, true, true, engaged);      // raw press
    logic.update(30, true, true, engaged);      // press edge
    logic.update(1000, false, true, engaged);   // raw release
    logic.update(1020, false, true, engaged);   // release edge -> recovery
    ButtonGestureOutput o = logic.update(2000, false, true, false); // throttle closed mid-recovery
    assert(o.intent != ButtonIntent::Disarm);
    assert(o.powerScale > 0);                   // ramp keeps recovering
    cout << "PASS: closing the throttle during recovery does not disarm\n";
}

void test_aborted_hold_resets_arm_charge() {
    // Regression: a short release out of ArmCharging used to leave a stale
    // non-zero armCharge behind, so SoundState::ArmCharging (reps=0) would
    // keep playing forever. The charge must reset on every abort, however
    // briefly the button was held.
    ButtonGestureLogic logic;
    const bool engaged = true;
    logic.update(0, true, false, engaged);
    logic.update(20, true, false, engaged);
    logic.update(100, false, false, engaged);
    logic.update(120, false, false, engaged);   // Click, window open
    logic.update(200, true, false, engaged);
    logic.update(220, true, false, engaged);    // press edge -> ArmCharging
    ButtonGestureOutput o = logic.update(260, true, false, engaged);
    assert(o.armCharge > 0);                    // charging...
    o = logic.update(300, false, false, engaged);
    o = logic.update(320, false, false, engaged); // release edge -> aborted hold
    assert(o.intent == ButtonIntent::None);     // swallowed, not a fresh click
    assert(o.armCharge == 0);                   // reset on release
    o = logic.update(10000, false, false, engaged);
    assert(o.armCharge == 0);                   // and stays 0
    cout << "PASS: an aborted hold resets armCharge to 0, even after a brief release\n";
}

void test_millis_rollover() {
    ButtonGestureLogic logic;
    const bool engaged = true;
    const uint32_t nearMax = 0xFFFFFFE0u;

    // Click across the wraparound: raw press just before it, edges straddle it.
    logic.update(nearMax, true, false, engaged);
    logic.update(nearMax + 20, true, false, engaged);                 // press edge
    logic.update(nearMax + 60, false, false, engaged);                // raw release
    ButtonGestureOutput o = logic.update(nearMax + 80, false, false, engaged); // release edge -> Click
    assert(o.intent == ButtonIntent::Click);

    // Arm window computed with unsigned subtraction still spans correctly.
    uint32_t pressRaw = 0x200u;
    logic.update(pressRaw, true, false, engaged);
    logic.update(pressRaw + 20, true, false, engaged);                // press edge (within window)
    o = logic.update(pressRaw + 20 + 2000, true, false, engaged);     // 2000 ms charge
    assert(o.intent == ButtonIntent::Arm);
    cout << "PASS: unsigned timing survives millis() rollover\n";
}

int main() {
    test_parity_arm_gesture();
    test_arm_window_boundary();
    test_aborted_hold_denies_immediate_rearm();
    test_aborted_hold_requires_new_click();
    test_short_abort_does_not_reopen_window();
    test_disarm_ramp_timing();
    test_disarm_ramp_recovery_and_inversion();
    test_no_power_disarms_immediately();
    test_engagement_drop_mid_ramp_disarms();
    test_release_after_disarm_and_stuck_button();
    test_external_disarm_mid_ramp_resets_scale();
    test_debounce_suppresses_short_pulses();
    test_millis_rollover();
    test_ramp_progresses_with_small_ticks();
    test_engagement_drop_during_recovery_does_not_disarm();
    test_aborted_hold_resets_arm_charge();
    cout << "ButtonGestureLogicTest: all passed" << endl;
    return 0;
}
