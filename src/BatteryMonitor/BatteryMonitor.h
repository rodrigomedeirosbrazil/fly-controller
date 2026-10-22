#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#include <Arduino.h>

class BatteryMonitor {
public:
    BatteryMonitor();

    // Initialization
    void init();

    // Update (call from main loop)
    void update();

    // Public SoC access methods
    uint8_t getSoC();                  // Returns 0-100%
    uint8_t getSoCFromVoltage();       // Returns 0-100% based on voltage only
    // Usable mAh above the reserve that this pack voltage corresponds to, in
    // the coulomb-count window [SoC(batMinV), 100] -- the same scale as
    // getRemainingMah() with current, so the two can be compared directly.
    uint32_t usableMahFromVoltage(uint16_t batteryMilliVolts) const;
    uint16_t getRemainingMah();        // Returns mAh remaining
    uint16_t getConsumedMah();         // Returns mAh consumed
    uint16_t getCapacity();            // Capacity in use, mAh (BMS-reported, else configured)
    // The configured capacity (CFG_SET). Resets to full when it becomes the
    // capacity in use; while a BMS capacity is in use it is only stored.
    void setCapacity(uint16_t mAh);
    // The BMS-reported capacity (BmsCapacitySync). Runtime only, wins over the
    // configured one, and keeps the SoC % instead of resetting to full.
    void setBmsCapacity(uint16_t mAh);
    uint16_t getBmsCapacity() const;   // Adopted BMS capacity, 0 = none this boot
    void resetToFullCharge();          // Reset to 100% charge

private:
    // Coulomb counting state
    uint32_t batteryCapacityMilliAh;  // Capacity in use, mAh
    uint32_t bmsCapacityMilliAh;      // From the BMS, 0 = none reported this boot
    uint32_t usableCapacityMilliAh;   // Usable capacity in mAh (internal reserve applied)
    uint32_t reserveMilliAh;          // Reserved capacity in mAh
    uint32_t remainingMilliAh;         // Remaining capacity in mAh
    unsigned long lastCoulombTs;       // Timestamp of last Coulomb update
    uint64_t coulombRemainderMaMs;     // Remainder accumulator (mA*ms) for precision
    unsigned long zeroCurrentStartMs;  // Start time for no-load detection

    // Internal methods
    void updateCoulombCount();
    void recalibrateFromVoltage();
    uint8_t estimateSoCFromVoltageLiPo(uint16_t batteryMilliVolts) const;
    uint8_t estimateSoCFromConfiguredVoltageRange(uint16_t batteryMilliVolts) const;
    void getConfiguredSocWindow(uint8_t& socMin, uint8_t& socMax) const;
    uint32_t remainingMahFromScaledSoc(uint8_t scaledSoc) const;
    uint8_t scaledSocFromVoltage(uint16_t batteryMilliVolts) const;
    void updateUsableCapacity();
    uint32_t getUsableRemainingMah() const;
};

#endif // BATTERY_MONITOR_H
