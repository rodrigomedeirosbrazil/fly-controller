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

    // Inside: click release edge @120, press edge @700 (580 ms gap) -> arms.
    ButtonGestureLogic inside;
    inside.update(0,   true,  false, engaged);
    inside.update(20,  true,  false, engaged);                        // press edge
    inside.update(100, false, false, engaged);
    ButtonGestureOutput o = inside.update(120, false, false, engaged); // release edge -> Click
    assert(o.intent == ButtonIntent::Click);
    inside.update(680, true, false, engaged);                         // raw press
    inside.update(700, true, false, engaged);                         // press edge @700
    o = inside.update(2700, true, false, engaged);                    // 2000 ms of charge
    assert(o.intent == ButtonIntent::Arm);

    // Outside: same click, press edge @720 (600 ms gap) -> window already gone.
    ButtonGestureLogic outside;
    outside.update(0,   true,  false, engaged);
    outside.update(20,  true,  false, engaged);
    outside.update(100, false, false, engaged);
    o = outside.update(120, false, false, engaged);                   // release edge -> Click
    assert(o.intent == ButtonIntent::Click);
    outside.update(700, true, false, engaged);                        // raw press
    outside.update(720, true, false, engaged);                        // press edge @720
    o = outside.update(2720, true, false, engaged);                   // held 2000 ms
    assert(o.intent == ButtonIntent::None);                           // never arms
    o = outside.update(2800, false, false, engaged);
    o = outside.update(2820, false, false, engaged);                  // release, not a click
    assert(o.intent == ButtonIntent::None);
    cout << "PASS: the arming window closes at BUTTON_ARM_WINDOW_MS\n";
}

void test_partial_holds_do_not_sum() {
    // A release out of ArmCharging that doesn't itself qualify as a click
    // leaves clickReleaseMs_ untouched (stale, from the original click) — so
    // both partial holds below have to land inside the *original* 600 ms
    // window, not just close to each other.
    ButtonGestureLogic logic;
    const bool engaged = true;

    logic.update(0, true, false, engaged);
    logic.update(20, true, false, engaged);
    logic.update(100, false, false, engaged);
    logic.update(120, false, false, engaged);                         // click, window open (closes @720)

    // First partial hold: 400 ms, then release.
    logic.update(140, true, false, engaged);
    logic.update(160, true, false, engaged);                          // charge starts
    ButtonGestureOutput o = logic.update(560, true, false, engaged);
    assert(o.armCharge == 20);                                        // 400/2000 ms charged
    o = logic.update(580, false, false, engaged);                     // raw release
    o = logic.update(600, false, false, engaged);                     // release edge (not a click: 440 ms held)
    assert(o.armCharge == 0);                                         // resets on release

    // Second partial hold: same 400 ms, still inside the original window. Same charge, never sums to Arm.
    logic.update(620, true, false, engaged);
    logic.update(640, true, false, engaged);                          // charge starts fresh
    o = logic.update(1040, true, false, engaged);
    assert(o.armCharge == 20);
    assert(o.intent != ButtonIntent::Arm);
    cout << "PASS: releasing the arm charge resets it; partial holds do not sum\n";
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

void test_short_click_during_window_resets_arm_charge() {
    // Regression: a short click on the ArmCharging path used to leave a stale
    // non-zero armCharge behind, so SoundState::ArmCharging (reps=0) would
    // keep playing forever. The charge must reset on every release.
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
    o = logic.update(320, false, false, engaged); // release edge -> short click
    assert(o.intent == ButtonIntent::Click);
    assert(o.armCharge == 0);                   // reset on release
    o = logic.update(10000, false, false, engaged);
    assert(o.armCharge == 0);                   // and stays 0
    cout << "PASS: a short click inside the window resets armCharge to 0\n";
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
    test_partial_holds_do_not_sum();
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
    test_short_click_during_window_resets_arm_charge();
    cout << "ButtonGestureLogicTest: all passed" << endl;
    return 0;
}
