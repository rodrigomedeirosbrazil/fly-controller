#pragma once

#include <stdint.h>

// Remaining charge after a capacity change that says nothing about the
// charge itself (a BMS reporting its nominal capacity): keep the fraction.
// Contrast BatteryMonitor::setCapacity(), which resets to full because a
// pilot typing a capacity has just fitted a charged pack.
//
// oldCapacityMah == 0 means there was nothing to keep: report full.
// 64-bit intermediate: the product exceeds 32 bits for large packs.
inline uint32_t rescaleRemainingMah(uint32_t remainingMah,
                                    uint32_t oldCapacityMah,
                                    uint32_t newCapacityMah) {
    if (oldCapacityMah == 0) {
        return newCapacityMah;
    }
    const uint64_t scaled = (uint64_t) remainingMah * newCapacityMah / oldCapacityMah;
    return scaled > newCapacityMah ? newCapacityMah : (uint32_t) scaled;
}
