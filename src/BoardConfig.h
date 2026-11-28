#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include <stdbool.h>
#include <stdint.h>
#include "ThrottleOutput/ThrottleOutputLogic.h"

/**
 * Runtime board/controller configuration.
 * Centralizes build-specific flags and defaults to reduce #if scattered across the codebase.
 */
struct BoardConfig {
    bool hasCurrentSensor;         // false=XAG, true=Tmotor/Hobbywing (Coulomb counting)
    ThrottleOutputConfig throttleOutput;  // soft start, offset, decel, stop rules (spec section 6.3)
    bool useBatteryLimit;
    uint16_t defaultBatteryCapacity;
    int32_t defaultEscMaxTemp;
    int32_t defaultEscTempReductionStart;
    uint16_t defaultReferenceCurrentA;  // seed of the remaining-flight-time average, A
};

const BoardConfig& getBoardConfig();

#endif // BOARD_CONFIG_H
