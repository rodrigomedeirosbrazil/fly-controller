#include "BatteryMonitor.h"
#include "../config.h"
#include "../BoardConfig.h"
#include "../Telemetry/TelemetryAvailability.h"

extern Settings settings;

BatteryMonitor::BatteryMonitor() {
    // Capacity will be set from Settings in init()
    batteryCapacityMilliAh = 0;
    remainingMilliAh = 0;
    lastCoulombTs = 0;
}

void BatteryMonitor::init() {
    // Load capacity from Settings
    batteryCapacityMilliAh = settings.getBatteryCapacityMah();
    remainingMilliAh = batteryCapacityMilliAh;
}

void BatteryMonitor::update() {
    updateCoulombCount();
}

void BatteryMonitor::updateCoulombCount() {
    if (!isCurrentAvailable()) {
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
    if (telemetry.getBatteryCurrentMilliAmps() == 0) {
        recalibrateFromVoltage();
        lastCoulombTs = currentTs;
        return;
    }

    // Calculate time delta in milliseconds
    unsigned long deltaMs = currentTs - lastCoulombTs;

    // Calculate mAh consumed: ΔmAh = (I(mA) * Δt(ms)) / 3600000
    uint32_t deltaMilliAh = (telemetry.getBatteryCurrentMilliAmps() * deltaMs) / 3600000;

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
    if (!telemetry.hasData()) {
        return;
    }

    uint16_t batteryMilliVolts = telemetry.getBatteryVoltageMilliVolts();
    uint8_t voltagePercentage = estimateSoCFromVoltageLiPo(batteryMilliVolts);
    uint32_t voltageBasedRemaining = ((uint32_t)voltagePercentage * batteryCapacityMilliAh) / 100;

    if (getBoardConfig().hasCurrentSensor) {
        // Hobbywing/Tmotor: smooth recalibration 90% Coulomb + 10% voltage-based
        remainingMilliAh = (remainingMilliAh * 9 + voltageBasedRemaining) / 10;
    } else {
        // XAG: voltage-only
        remainingMilliAh = voltageBasedRemaining;
    }

    if (remainingMilliAh > batteryCapacityMilliAh) {
        remainingMilliAh = batteryCapacityMilliAh;
    }
}

uint8_t BatteryMonitor::getSoC() {
    if (getBoardConfig().hasCurrentSensor) {
        if (batteryCapacityMilliAh == 0) {
            return 0;
        }
        uint32_t soc = (remainingMilliAh * 100) / batteryCapacityMilliAh;
        return constrain((uint8_t)soc, 0, 100);
    }
    if (!telemetry.hasData()) {
        return 0;
    }
    return estimateSoCFromVoltageLiPo(telemetry.getBatteryVoltageMilliVolts());
}

uint8_t BatteryMonitor::getSoCFromVoltage() {
    if (!telemetry.hasData()) {
        return 0;
    }
    return estimateSoCFromVoltageLiPo(telemetry.getBatteryVoltageMilliVolts());
}

uint16_t BatteryMonitor::getRemainingMah() {
    if (getBoardConfig().hasCurrentSensor) {
        return (uint16_t)remainingMilliAh;
    }
    uint8_t soc = getSoC();
    return (uint16_t)((batteryCapacityMilliAh * soc) / 100);
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

