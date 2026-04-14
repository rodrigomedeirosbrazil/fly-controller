#include "BatteryMonitor.h"
#include "../config.h"
#include "../Telemetry/TelemetryAvailability.h"

extern Settings settings;

BatteryMonitor::BatteryMonitor() {
    // Capacity will be set from Settings in init()
    batteryCapacityMilliAh = 0;
    usableCapacityMilliAh = 0;
    reserveMilliAh = 0;
    remainingMilliAh = 0;
    lastCoulombTs = 0;
    coulombRemainderMaMs = 0;
    zeroCurrentStartMs = 0;
}

void BatteryMonitor::init() {
    // Load capacity from Settings
    batteryCapacityMilliAh = settings.getBatteryCapacityMah();
    updateUsableCapacity();
    remainingMilliAh = batteryCapacityMilliAh;
}

void BatteryMonitor::update() {
    updateUsableCapacity();
    updateCoulombCount();
}

void BatteryMonitor::updateCoulombCount() {
    unsigned long currentTs = millis();

    if (!isCurrentAvailable()) {
        lastCoulombTs = currentTs;
        zeroCurrentStartMs = 0;
        return;
    }

    // Initialize timestamp on first call
    if (lastCoulombTs == 0) {
        lastCoulombTs = currentTs;
        zeroCurrentStartMs = 0;
        recalibrateFromVoltage();
        return;
    }

    // Check if we should recalibrate from voltage (no load condition)
    const uint32_t currentMa = telemetry.getBatteryCurrentMilliAmps();
    if (currentMa <= BATTERY_RECALIBRATION_CURRENT_MA) {
        if (zeroCurrentStartMs == 0) {
            zeroCurrentStartMs = currentTs;
        } else if (currentTs - zeroCurrentStartMs >= BATTERY_RECALIBRATION_STABLE_MS) {
            recalibrateFromVoltage();
            coulombRemainderMaMs = 0;
            lastCoulombTs = currentTs;
            // Restart the stable window; otherwise time since the original low-current
            zeroCurrentStartMs = currentTs;
            return;
        }
    } else {
        zeroCurrentStartMs = 0;
    }

    // Calculate time delta in milliseconds
    unsigned long deltaMs = currentTs - lastCoulombTs;

    // Calculate mAh consumed: ΔmAh = (I(mA) * Δt(ms)) / 3600000
    uint64_t deltaMaMs = (uint64_t)currentMa * (uint64_t)deltaMs;
    coulombRemainderMaMs += deltaMaMs;
    uint32_t deltaMilliAh = (uint32_t)(coulombRemainderMaMs / 3600000ULL);
    coulombRemainderMaMs %= 3600000ULL;

    // Subtract from remaining capacity
    if (deltaMilliAh <= remainingMilliAh) {
        remainingMilliAh -= deltaMilliAh;
    } else {
        remainingMilliAh = 0;  // Prevent underflow
    }

    // Constrain to valid range
    if (remainingMilliAh > batteryCapacityMilliAh) {
        remainingMilliAh = batteryCapacityMilliAh;
    }

    lastCoulombTs = currentTs;
}

uint8_t BatteryMonitor::estimateSoCFromVoltageLiPo(uint16_t batteryMilliVolts) const {
    // Calculate voltage per cell
    uint16_t cellMilliVolts = batteryMilliVolts / BATTERY_CELL_COUNT;

    // LiPo discharge curve lookup table
    struct VoltToSoC {
        uint16_t mv;
        uint8_t soc;
    };

    // Real LiPo discharge curve: voltage drops quickly from 4.20V to ~4.00V
    // (surface charge effect) but little actual capacity is consumed in that region.
    // The main capacity plateau spans 4.00V down to ~3.40V, then drops steeply.
    const VoltToSoC curve[] = {
        {4200, 100}, {4175, 99}, {4150, 97}, {4125, 94},
        {4100, 91},  {4075, 88}, {4050, 87},
        {4000, 85},  // plateau begins ~4.00V; most capacity lives below this
        {3950, 79},  {3900, 72}, {3850, 64},
        {3800, 56},  {3750, 48}, {3700, 40},
        {3650, 32},  {3600, 24}, {3550, 17},
        {3500, 11},  {3450, 6},  {3400, 3},
        {3300, 1},   {3200, 0}
    };

    const int curveSize = sizeof(curve) / sizeof(VoltToSoC);

    // Linear interpolation between curve points
    for (int i = 0; i < curveSize - 1; i++) {
        uint16_t v1 = curve[i].mv;
        uint16_t v2 = curve[i + 1].mv;

        if (cellMilliVolts >= v2 && cellMilliVolts <= v1) {
            uint8_t soc1 = curve[i].soc;
            uint8_t soc2 = curve[i + 1].soc;

            // Linear interpolation
            int32_t soc = soc1 - ((int32_t)(v1 - cellMilliVolts) * (soc1 - soc2)) / (v1 - v2);
            return constrain(soc, 0, 100);
        }
    }

    // Out of range
    return (cellMilliVolts > 4200) ? 100 : 0;
}

void BatteryMonitor::getConfiguredSocWindow(uint8_t& socMin, uint8_t& socMax) const {
    uint16_t minMv = settings.getBatteryMinVoltage();
    uint16_t maxMv = settings.getBatteryMaxVoltage();
    if (maxMv < minMv) {
        uint16_t tmp = maxMv;
        maxMv = minMv;
        minMv = tmp;
    }

    socMin = estimateSoCFromVoltageLiPo(minMv);
    socMax = estimateSoCFromVoltageLiPo(maxMv);
    if (socMax < socMin) {
        socMax = socMin;
    }
}

uint8_t BatteryMonitor::estimateSoCFromConfiguredVoltageRange(uint16_t batteryMilliVolts) const {
    uint8_t socMin = 0;
    uint8_t socMax = 100;
    getConfiguredSocWindow(socMin, socMax);

    if (socMax <= socMin) {
        return 0;
    }

    uint8_t absoluteSoc = estimateSoCFromVoltageLiPo(batteryMilliVolts);
    if (absoluteSoc <= socMin) {
        return 0;
    }
    if (absoluteSoc >= socMax) {
        return 100;
    }

    uint32_t num = (uint32_t)(absoluteSoc - socMin) * 100U;
    uint32_t den = (uint32_t)(socMax - socMin);
    return (uint8_t)(num / den);
}

uint32_t BatteryMonitor::remainingMahFromScaledSoc(uint8_t scaledSoc) const {
    if (scaledSoc > 100) {
        scaledSoc = 100;
    }
    uint32_t usableFromSoc = ((uint32_t)scaledSoc * usableCapacityMilliAh) / 100U;
    uint32_t remaining = reserveMilliAh + usableFromSoc;
    if (remaining > batteryCapacityMilliAh) {
        remaining = batteryCapacityMilliAh;
    }
    return remaining;
}

void BatteryMonitor::recalibrateFromVoltage() {
    if (!telemetry.hasData()) {
        return;
    }

    uint16_t batteryMilliVolts = telemetry.getBatteryVoltageMilliVolts();
    uint8_t scaledSoc = estimateSoCFromConfiguredVoltageRange(batteryMilliVolts);
    uint32_t voltageBasedRemaining = remainingMahFromScaledSoc(scaledSoc);

    if (isCurrentAvailable()) {
        // Current available: smooth recalibration 90% Coulomb + 10% voltage-based
        remainingMilliAh = (remainingMilliAh * 9 + voltageBasedRemaining) / 10;
    } else {
        // No current source available: voltage-only
        remainingMilliAh = voltageBasedRemaining;
    }

    if (remainingMilliAh > batteryCapacityMilliAh) {
        remainingMilliAh = batteryCapacityMilliAh;
    }
}

uint8_t BatteryMonitor::getSoC() {
    if (isCurrentAvailable()) {
        if (usableCapacityMilliAh == 0) {
            return 0;
        }
        uint32_t usableRemaining = getUsableRemainingMah();
        uint32_t soc = (usableRemaining * 100) / usableCapacityMilliAh;
        return constrain((uint8_t)soc, 0, 100);
    }
    if (!telemetry.hasData()) {
        return 0;
    }
    return estimateSoCFromConfiguredVoltageRange(telemetry.getBatteryVoltageMilliVolts());
}

uint8_t BatteryMonitor::getSoCFromVoltage() {
    if (!telemetry.hasData()) {
        return 0;
    }
    return estimateSoCFromConfiguredVoltageRange(telemetry.getBatteryVoltageMilliVolts());
}

uint16_t BatteryMonitor::getRemainingMah() {
    if (isCurrentAvailable()) {
        return (uint16_t)getUsableRemainingMah();
    }
    uint8_t soc = getSoC();
    return (uint16_t)(((uint32_t)usableCapacityMilliAh * soc) / 100U);
}

uint16_t BatteryMonitor::getConsumedMah() {
    if (usableCapacityMilliAh == 0) {
        return 0;
    }
    uint32_t remaining = getUsableRemainingMah();
    return (uint16_t)(usableCapacityMilliAh - remaining);
}

uint16_t BatteryMonitor::getCapacity() {
    return (uint16_t)batteryCapacityMilliAh;
}

void BatteryMonitor::setCapacity(uint16_t mAh) {
    batteryCapacityMilliAh = mAh;
    updateUsableCapacity();
    remainingMilliAh = mAh;  // Reset to full charge
    coulombRemainderMaMs = 0;
    zeroCurrentStartMs = 0;
}

void BatteryMonitor::resetToFullCharge() {
    remainingMilliAh = batteryCapacityMilliAh;
    coulombRemainderMaMs = 0;
    zeroCurrentStartMs = 0;
}

void BatteryMonitor::updateUsableCapacity() {
    if (batteryCapacityMilliAh == 0) {
        usableCapacityMilliAh = 0;
        reserveMilliAh = 0;
        return;
    }

    uint8_t socMin = 0;
    uint8_t socMax = 100;
    getConfiguredSocWindow(socMin, socMax);

    uint32_t reserve = ((uint32_t)batteryCapacityMilliAh * socMin) / 100;
    uint32_t usable = ((uint32_t)batteryCapacityMilliAh * (socMax - socMin)) / 100;

    if (reserve > batteryCapacityMilliAh) {
        reserve = batteryCapacityMilliAh;
    }
    if (usable > batteryCapacityMilliAh - reserve) {
        usable = batteryCapacityMilliAh - reserve;
    }

    reserveMilliAh = reserve;
    usableCapacityMilliAh = usable;
}

uint32_t BatteryMonitor::getUsableRemainingMah() const {
    if (remainingMilliAh <= reserveMilliAh) {
        return 0;
    }
    uint32_t usableRemaining = remainingMilliAh - reserveMilliAh;
    if (usableRemaining > usableCapacityMilliAh) {
        usableRemaining = usableCapacityMilliAh;
    }
    return usableRemaining;
}
