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
    // Every BMS driver clears its data flags when the link drops, so
    // availability alone is enough to keep a stale alert from holding on.
    BmsAlertReading reading;
    reading.cellDataAvailable = isBmsCellDataAvailable();
    reading.cellMinMv         = bluetoothBms.getCellMinMilliVolts();
    reading.cellDeltaMv       = bluetoothBms.getCellDeltaMilliVolts();

    const uint8_t tempCount = bluetoothBms.getTempCount();
    reading.tempAvailable = isBmsDataAvailable() && tempCount > 0;
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
