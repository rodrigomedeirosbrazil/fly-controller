#include "BoardConfig.h"
#include "config_controller.h"

#if IS_XAG
// XAG: no current sensor, soft start, battery limit, 18Ah, 80/70°C ESC, 75 A reference
static const BoardConfig s_config = {
    false, {
        // XAG: wake-up 1.5 s at 5%, then 5% -> 20% in 2 s (bench 2026-11-28: 3 s felt steppy); coast stop.
        // Decel 2 s full to minimum (bench 2026-11-28: 1 s was not noticeable; protects the ESC
        // more against regen from full throttle). Provisional, see spec section 8.
        true, 1500, 50000, 200000, 75000,
        false, 0, 0,
        0, 500000,
        false, 0, 800
    }, true,
    18000, 80000, 70000,
    75
};
#elif IS_TMOTOR
// Tmotor: current via CAN, soft start, 18Ah, 110/80°C ESC, 90 A reference
static const BoardConfig s_config = {
    true, {
        // T-Motor: ramp to 20%, release by RPM or time; no running offset (15% on the
        // bench 2026-11-28 jumped after release and kept the motor turning on a
        // rest reading that drifted above the release point). Provisional, see spec section 8.
        true, 0, 0, 200000, 66667,
        true, 600, 4000,
        0, 1000000,
        true, 500, 3000
    }, true,
    18000, 110000, 80000,
    90
};
#elif IS_HOBBYWING
// Hobbywing: current via CAN, no soft start, 60Ah, 110/80°C ESC, 90 A reference
static const BoardConfig s_config = {
    true, {
        // Hobbywing: no soft start, direct tracking. Provisional, see spec section 8.
        false, 0, 0, 0, 0,
        false, 0, 0,
        0, 1000000,
        true, 500, 3000
    }, true,
    60000, 110000, 80000,
    90
};
#else
static const BoardConfig s_config = {
    false, {
        false, 0, 0, 0, 0,
        false, 0, 0,
        0, 1000000,
        false, 0, 800
    }, false,
    18000, 110000, 80000,
    90
};
#endif

const BoardConfig& getBoardConfig() {
    return s_config;
}
