#include "BoardConfig.h"
#include "config_controller.h"

#if IS_XAG
// XAG: no current sensor, smooth start active, battery limit, 18Ah, 80/70°C ESC
static const BoardConfig s_config = {
    false, true, true,
    18000, 80000, 70000
};
#elif IS_HOBBYWING
// Hobbywing: current from CAN, no smooth start, 65Ah, 110/80°C ESC
static const BoardConfig s_config = {
    true, false, true,
    65000, 110000, 80000
};
#elif IS_TMOTOR
// Tmotor: current via CAN, no smooth start, 18Ah, 110/80°C ESC
static const BoardConfig s_config = {
    true, false, true,
    18000, 110000, 80000
};
#else
static const BoardConfig s_config = {
    false, false, false,
    18000, 110000, 80000
};
#endif

const BoardConfig& getBoardConfig() {
    return s_config;
}
