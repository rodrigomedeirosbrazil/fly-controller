// src/Button/ButtonGestureLogic.h
#pragma once
#include <stdint.h>
#include <stdbool.h>

// Pure button-gesture policy — no Arduino deps, host-testable. Same shape as
// ThrottleSignalLogic: one `update()` per tick fed by raw inputs, all timing
// state kept inside.
//
// The gesture is a single scalar per direction, not a phase machine with
// special "abort" states:
//
//   armCharge : 0..100   disarmed side. Rises 0->100 over 2000 ms while held,
//                        resets to 0 on release. Arming requires one
//                        continuous intent — two partial holds do not sum.
//   powerScale: 0..100   armed side. 100 = no ramp. Falls at 100%/2000 ms while
//                        pressed, rises at 100%/2000 ms on release (symmetric
//                        recovery). Reaching 0 disarms. While disarmed it is
//                        always 100.
//
// Arming gesture (unchanged from today): a qualifying short click (press +
// release within 300 ms) opens a 3500 ms arming window; a press starting inside
// that window and held 2000 ms emits Arm. Every qualifying click emits Click —
// including the one that opens the window — so the click beeps, then the hold
// arms, exactly as before.
//
// No power, no ramp (3.2): with throttle un-engaged a press disarms
// immediately, and engagement dropping while a ramp is in progress also
// disarms immediately. The rule is evaluated continuously, not only at the
// press edge.
//
// Debounce (20 ms) lives here rather than in a button library so the one piece
// of this subsystem that needs host coverage is inside the harness. Debounced
// edges feed the gesture; pulses shorter than 20 ms produce no intent.
//
// Time comparisons use unsigned subtraction, correct across millis() rollover,
// as SoundLogic::PhaseRunner already does.
enum class ButtonIntent : uint8_t {
    None,
    Click,   // qualifying short click — beep
    Arm,     // arming hold completed — throttle.setArmed()
    Disarm,  // ramp reached 0, or immediate (no-power) disarm
};

struct ButtonGestureOutput {
    ButtonIntent intent;
    uint8_t      armCharge;   // 0..100, disarmed side
    uint8_t      powerScale;  // 0..100, armed side; 100 = no ramp
};

class ButtonGestureLogic {
public:
    // All carry today's values so phase 1 is behaviour-neutral.
    static constexpr uint32_t BUTTON_DEBOUNCE_MS         = 20;
    static constexpr uint32_t BUTTON_CLICK_MAX_MS        = 300;
    static constexpr uint32_t BUTTON_ARM_WINDOW_MS       = 3500;
    static constexpr uint32_t BUTTON_ARM_CHARGE_MS       = 2000;
    static constexpr uint32_t BUTTON_DISARM_RAMP_DOWN_MS = 2000;
    static constexpr uint32_t BUTTON_DISARM_RAMP_UP_MS   = 2000;

    ButtonGestureLogic()
        : debouncedPressed_(false),
          debouncedPrev_(false),
          debouncePending_(false),
          debounceChangeMs_(0),
          pressStartMs_(0),
          clickReleaseMs_(0),
          chargeStartMs_(0),
          state_(GestureState::Idle),
          armCharge_(0),
          powerScale_(100),
          rampAnchorMs_(0),
          pressConsumed_(false),
          prevArmed_(false) {}

    ButtonGestureOutput update(uint32_t nowMs, bool rawPressed, bool armed, bool engaged) {
        debounce(rawPressed, nowMs);
        bool pressEdge   =  debouncedPressed_ && !debouncedPrev_;
        bool releaseEdge = !debouncedPressed_ &&  debouncedPrev_;
        debouncedPrev_ = debouncedPressed_;
        if (pressEdge)   pressStartMs_ = nowMs;
        if (releaseEdge) pressConsumed_ = false;

        ButtonGestureOutput out;
        out.intent     = ButtonIntent::None;
        out.armCharge  = 0;
        out.powerScale = 100;

        if (armed) {
            // -- Armed: disarm ramp -----------------------------------------
            if (!prevArmed_) {
                powerScale_ = 100;
                rampAnchorMs_ = nowMs;
                if (debouncedPressed_) pressConsumed_ = true;
            }
            if (pressEdge || releaseEdge) rampAnchorMs_ = nowMs;
            uint32_t elapsed = nowMs - rampAnchorMs_;
            rampAnchorMs_ = nowMs;

            // 3.2, continuous: no power, no ramp. A press with the throttle
            // un-engaged disarms immediately; so does engagement dropping while
            // a ramp is actually in progress. powerScale never leaves 100 here.
            if (!engaged && (pressEdge || powerScale_ < 100)) {
                out.intent = ButtonIntent::Disarm;
                powerScale_ = 100;
                if (debouncedPressed_) pressConsumed_ = true;
            } else if (debouncedPressed_ && !pressConsumed_) {
                // Falls while held. Reaching 0 disarms (latch) and resets.
                uint32_t fall = scaleStep(elapsed, BUTTON_DISARM_RAMP_DOWN_MS);
                if (fall >= powerScale_) {
                    out.intent     = ButtonIntent::Disarm;
                    out.powerScale = 0;
                    powerScale_    = 100;
                    pressConsumed_ = true;
                } else {
                    powerScale_ -= fall;
                }
            } else if (!debouncedPressed_ && powerScale_ < 100) {
                // Symmetric recovery. Never instantaneous: restoring full
                // thrust in one tick with the throttle open is the one way
                // this feature could hurt someone.
                uint32_t rise = scaleStep(elapsed, BUTTON_DISARM_RAMP_UP_MS);
                powerScale_ = (powerScale_ + rise >= 100) ? 100 : (powerScale_ + rise);
            }
            // A ramp reaching 0 reports 0 on the disarm tick even though the
            // internal integrator is already reset for the next session.
            out.powerScale = (out.intent == ButtonIntent::Disarm) ? out.powerScale : powerScale_;
        } else {
            // -- Disarmed: click + arm charge -------------------------------
            powerScale_ = 100;

            // An external (fault) disarm mid-ramp resets the ramp and consumes
            // the held press so it cannot immediately start a fresh gesture.
            if (prevArmed_) {
                state_ = GestureState::Idle;
                armCharge_ = 0;
                rampAnchorMs_ = 0;
                if (debouncedPressed_) pressConsumed_ = true;
            }

            switch (state_) {
            case GestureState::Idle:
                if (releaseEdge && (nowMs - pressStartMs_ <= BUTTON_CLICK_MAX_MS)) {
                    out.intent     = ButtonIntent::Click;
                    clickReleaseMs_ = nowMs;
                    state_         = GestureState::ClickPending;
                }
                break;

            case GestureState::ClickPending:
                if (nowMs - clickReleaseMs_ >= BUTTON_ARM_WINDOW_MS) {
                    state_ = GestureState::Idle;
                    break;
                }
                if (pressEdge) {
                    chargeStartMs_ = nowMs;
                    state_         = GestureState::ArmCharging;
                }
                break;

            case GestureState::ArmCharging:
                if (!debouncedPressed_) {
                    // Released before the charge completed. A press this short
                    // is a click and re-opens the arming window; anything
                    // longer just resets the charge — two partial holds do not
                    // sum.
                    if (nowMs - pressStartMs_ <= BUTTON_CLICK_MAX_MS) {
                        out.intent      = ButtonIntent::Click;
                        clickReleaseMs_ = nowMs;
                    } else {
                        armCharge_ = 0;
                    }
                    state_ = GestureState::ClickPending;
                } else if (nowMs - chargeStartMs_ >= BUTTON_ARM_CHARGE_MS) {
                    out.intent     = ButtonIntent::Arm;
                    armCharge_    = 0;
                    state_         = GestureState::Idle;
                    pressConsumed_ = true;
                } else {
                    armCharge_ = (uint8_t)((nowMs - chargeStartMs_) * 100 / BUTTON_ARM_CHARGE_MS);
                }
                break;
            }
            out.armCharge  = armCharge_;
            out.powerScale = 100;
        }

        prevArmed_ = armed;
        return out;
    }

    void reset() {
        *this = ButtonGestureLogic();
    }

private:
    enum class GestureState : uint8_t {
        Idle,          // disarmed, no arming window open
        ClickPending,  // disarmed, within 3500 ms of the last qualifying click
        ArmCharging,   // disarmed, holding to charge the arm
    };

    static uint32_t scaleStep(uint32_t elapsedMs, uint32_t spanMs) {
        if (spanMs == 0) return 0;
        return (uint32_t)((uint64_t)elapsedMs * 100 / spanMs);
    }

    // Standard level debounce: the debounced value only follows the raw value
    // after it has held steady for the debounce period. Pulses shorter than
    // the debounce period never commit an edge.
    void debounce(bool rawPressed, uint32_t nowMs) {
        if (rawPressed != debouncedPressed_) {
            if (!debouncePending_) {
                debouncePending_   = true;
                debounceChangeMs_ = nowMs;
            } else if (nowMs - debounceChangeMs_ >= BUTTON_DEBOUNCE_MS) {
                debouncedPressed_ = rawPressed;
                debouncePending_  = false;
            }
        } else {
            debouncePending_ = false;
        }
    }

    bool         debouncedPressed_;
    bool         debouncedPrev_;
    bool         debouncePending_;
    uint32_t     debounceChangeMs_;
    uint32_t     pressStartMs_;
    uint32_t     clickReleaseMs_;
    uint32_t     chargeStartMs_;
    GestureState state_;
    uint8_t      armCharge_;
    uint8_t      powerScale_;
    uint32_t     rampAnchorMs_;
    bool         pressConsumed_;
    bool         prevArmed_;
};
