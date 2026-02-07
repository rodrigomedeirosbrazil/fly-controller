#include "BatteryMonitor.h"
#include "../Telemetry/TelemetryProvider.h"
#include "../Telemetry/TelemetryData.h"
#include "../config.h"

extern TelemetryProvider* telemetry;

BatteryMonitor::BatteryMonitor() {
    // Set capacity based on controller type
    #if IS_HOBBYWING
    batteryCapacityMilliAh = 65000;  // 65.0 Ah for Hobbywing
    remainingMilliAh = 65000;
    #else
    batteryCapacityMilliAh = 18000;  // 18.0 Ah for Tmotor and XAG
    remainingMilliAh = 18000;
    #endif

    lastCoulombTs = 0;
}

void BatteryMonitor::init() {
    // Initialize if needed (currently nothing required)
}

void BatteryMonitor::update() {
    updateCoulombCount();
}

void BatteryMonitor::updateCoulombCount() {
    if (!telemetry || !telemetry->getData()) {
        return;
    }

    TelemetryData* data = telemetry->getData();

    #if IS_XAG
    // XAG mode: no current data available, skip Coulomb counting
    return;
    #else
    if (!data->hasTelemetry) {
        return;
    }

    unsigned long currentTs = millis();

    // Initialize timestamp on first call
    if (lastCoulombTs == 0) {
        lastCoulombTs = currentTs;
        recalibrateFromVoltage();
        return;
    }

    // Check if we should recalibrate from voltage (no load condition)
    if (data->batteryCurrentMilliAmps == 0) {
        recalibrateFromVoltage();
        lastCoulombTs = currentTs;
        return;
    }

    // Calculate time delta in milliseconds
    unsigned long deltaMs = currentTs - lastCoulombTs;

    // Calculate mAh consumed: ΔmAh = (I(mA) * Δt(ms)) / 3600000
    uint32_t deltaMilliAh = (data->batteryCurrentMilliAmps * deltaMs) / 3600000;

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
    #endif
}

uint8_t BatteryMonitor::estimateSoCFromVoltageLiPo(uint16_t batteryMilliVolts) {
    // Calculate voltage per cell
    uint16_t cellMilliVolts = batteryMilliVolts / BATTERY_CELL_COUNT;

    // LiPo discharge curve lookup table
    struct VoltToSoC {
        uint16_t mv;
        uint8_t soc;
    };

    const VoltToSoC curve[] = {
        {4200, 100}, {4150, 95}, {4100, 90}, {4050, 85},
        {4000, 80}, {3950, 75}, {3900, 70}, {3850, 65},
        {3800, 60}, {3750, 55}, {3700, 50}, {3650, 45},
        {3600, 40}, {3550, 35}, {3500, 30}, {3450, 25},
        {3400, 20}, {3350, 15}, {3300, 10}, {3250, 5},
        {3200, 2}, {3100, 0}
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

void BatteryMonitor::recalibrateFromVoltage() {
    if (!telemetry || !telemetry->getData()) {
        return;
    }

    TelemetryData* data = telemetry->getData();

    #if IS_XAG
    // XAG: use LiPo curve
    uint16_t batteryMilliVolts = data->batteryVoltageMilliVolts;
    uint8_t voltagePercentage = estimateSoCFromVoltageLiPo(batteryMilliVolts);
    remainingMilliAh = ((uint32_t)voltagePercentage * batteryCapacityMilliAh) / 100;
    #else
    if (!data->hasTelemetry) {
        return;
    }

    // Hobbywing/Tmotor: use LiPo curve for recalibration
    uint16_t batteryMilliVolts = data->batteryVoltageMilliVolts;
    uint8_t voltagePercentage = estimateSoCFromVoltageLiPo(batteryMilliVolts);

    // Smooth recalibration: 90% Coulomb counting + 10% voltage-based
    uint32_t voltageBasedRemaining = ((uint32_t)voltagePercentage * batteryCapacityMilliAh) / 100;
    remainingMilliAh = (remainingMilliAh * 9 + voltageBasedRemaining) / 10;

    // Constrain to valid range
    if (remainingMilliAh > batteryCapacityMilliAh) {
        remainingMilliAh = batteryCapacityMilliAh;
    }
    #endif
}

uint8_t BatteryMonitor::getSoC() {
    #if IS_XAG
    // XAG: use LiPo curve (no current data)
    if (!telemetry || !telemetry->getData()) {
        return 0;
    }
    return estimateSoCFromVoltageLiPo(telemetry->getData()->batteryVoltageMilliVolts);
    #else
    // Hobbywing/Tmotor: use Coulomb counting
    if (batteryCapacityMilliAh == 0) {
        return 0;
    }
    uint32_t soc = (remainingMilliAh * 100) / batteryCapacityMilliAh;
    return constrain((uint8_t)soc, 0, 100);
    #endif
}

uint16_t BatteryMonitor::getRemainingMah() {
    #if IS_XAG
    // XAG: estimate from voltage
    uint8_t soc = getSoC();
    return (uint16_t)((batteryCapacityMilliAh * soc) / 100);
    #else
    return (uint16_t)remainingMilliAh;
    #endif
}

uint16_t BatteryMonitor::getConsumedMah() {
    return (uint16_t)(batteryCapacityMilliAh - getRemainingMah());
}

uint16_t BatteryMonitor::getCapacity() {
    return (uint16_t)batteryCapacityMilliAh;
}

void BatteryMonitor::setCapacity(uint16_t mAh) {
    batteryCapacityMilliAh = mAh;
    remainingMilliAh = mAh;  // Reset to full charge
}

void BatteryMonitor::resetToFullCharge() {
    remainingMilliAh = batteryCapacityMilliAh;
}

