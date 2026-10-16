#include "BmsCapacitySync.h"
#include "../config.h"

void BmsCapacitySync::handle(bool isArmed) {
    BmsCapacitySyncInput in;
    in.nowMs          = millis();
    in.bmsCapacityMah = bluetoothBms.getDesignCapacityMah();
    in.configuredMah  = settings.getBatteryCapacityMah();
    in.armed          = isArmed;

    const BmsCapacitySyncDecision d = logic_.update(in);
    if (d.action == BmsCapacitySyncAction::Apply) {
        const uint16_t oldMah = settings.getBatteryCapacityMah();
        // validateCapacity() bounds d.mah to 65000, so it fits the uint16_t.
        settings.setBatteryCapacityMah((uint16_t) d.mah);
        settings.save();
        batteryMonitor.rescaleCapacity((uint16_t) d.mah);
        Serial.printf("[BmsCapSync] capacity %u -> %u mAh (from BMS)\n",
                      (unsigned) oldMah, (unsigned) d.mah);
    } else if (d.action == BmsCapacitySyncAction::OutOfRange) {
        Serial.printf("[BmsCapSync] BMS capacity %u mAh outside 1000-65000, keeping %u\n",
                      (unsigned) d.mah, (unsigned) in.configuredMah);
    }
}
