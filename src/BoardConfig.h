#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

/**
 * Runtime board/controller configuration.
 * Centralizes build-specific flags and defaults to reduce #if scattered across the codebase.
 */
struct BoardConfig {
    bool hasCurrentSensor;         // false=XAG, true=Hobbywing/Tmotor (Coulomb counting)
    bool useBatteryLimit;          // true=use battery voltage to limit power
    uint16_t defaultBatteryCapacity;  // mAh
    int32_t defaultEscMaxTemp;         // millicelsius
    int32_t defaultEscTempReductionStart;  // millicelsius
};

const BoardConfig& getBoardConfig();

#endif // BOARD_CONFIG_H
