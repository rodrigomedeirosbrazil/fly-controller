#include "BmsAlert.h"
#include <Arduino.h>
#include "../config.h"
#include "../Throttle/Throttle.h"
#include "../Sound/Sound.h"
#include "../RemoteLink/RemoteLink.h"
#include "../RemoteLink/RemoteLinkProtocol.h"
#include "../Telemetry/TelemetryAvailability.h"

extern Throttle throttle;
extern Sound sound;
extern RemoteLink remoteLink;

void BmsAlert::handle() {
    // The JBD and Daly drivers keep hasData_/hasCellData_ true after the BLE
    // link drops (only JK clears them), so availability alone would hold a
    // stale reading -- and a stale alert -- on. Gate on the live link too.
    const bool linked = bluetoothBms.isConnected();

    BmsAlertReading reading;
    reading.cellDataAvailable = linked && isBmsCellDataAvailable();
    reading.cellMinMv         = bluetoothBms.getCellMinMilliVolts();
    reading.cellDeltaMv       = bluetoothBms.getCellDeltaMilliVolts();

    const uint8_t tempCount = bluetoothBms.getTempCount();
    reading.tempAvailable = linked && isBmsDataAvailable() && tempCount > 0;
    reading.tempMaxC = 0;
    if (reading.tempAvailable) {
        int16_t maxC = bluetoothBms.getTempCelsius(0);
        for (uint8_t i = 1; i < tempCount; i++) {
            const int16_t c = bluetoothBms.getTempCelsius(i);
            if (c > maxC) maxC = c;
        }
        reading.tempMaxC = maxC;
    }

    BmsAlertThresholds thresholds;
    thresholds.cellCutoffMv = perCellCutoffMv(
        settings.getBatteryMinVoltage(), bluetoothBms.getCellCount());
    thresholds.lowCellDeltaGateMv = settings.getBmsAlertDeltaGateMv();
    thresholds.maxDeltaMv         = settings.getBmsAlertMaxDeltaMv();
    thresholds.tempLimitC         = settings.getBmsAlertTempLimitC();

    const uint32_t now = millis();
    const uint8_t mask = logic_.update(reading, thresholds, now);

    TempProtectionAlertLogic::Output out =
        soundLogic_.update(throttle.isArmed(), mask, now);

    if (out.playLost) {
        sound.play(SoundEvent::BmsAlert);
        remoteLink.requestBeep(RemoteBeep::Alert);
    }
    if (out.playRestored) {
        // The generic rising "all clear" motif.
        sound.play(SoundEvent::TempProtectionRestoredLow);
        sound.play(SoundEvent::TempProtectionRestoredHigh);
    }
}
