#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#include <Arduino.h>

class TelemetryProvider;
struct TelemetryData;

class BatteryMonitor {
public:
    BatteryMonitor();

    // Initialization
    void init();

    // Update (call from main loop)
    void update();

    // Public SoC access methods
    uint8_t getSoC();                  // Returns 0-100%
    uint16_t getRemainingMah();        // Returns mAh remaining
    uint16_t getConsumedMah();         // Returns mAh consumed
    uint16_t getCapacity();            // Returns battery capacity in mAh
    void setCapacity(uint16_t mAh);    // Configure battery capacity
    void resetToFullCharge();          // Reset to 100% charge

private:
    // Coulomb counting state
    uint32_t batteryCapacityMilliAh;  // Capacity in mAh (65Ah for Hobbywing, 18Ah for others)
    uint32_t remainingMilliAh;         // Remaining capacity in mAh
    unsigned long lastCoulombTs;       // Timestamp of last Coulomb update

    // Internal methods
    void updateCoulombCount();
    void recalibrateFromVoltage();
    uint8_t estimateSoCFromVoltageLiPo(uint16_t batteryMilliVolts);
};

#endif // BATTERY_MONITOR_H

