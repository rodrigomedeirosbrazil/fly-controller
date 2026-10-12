#include "RemainingTime.h"
#include "../config.h"
#include "../BatteryMonitor/BatteryMonitor.h"
#include "../Telemetry/Telemetry.h"
#include "../Telemetry/TelemetryAvailability.h"

void RemainingTime::init() {
    logic_.seed(settings.getReferenceCurrentA());
}

void RemainingTime::handle(bool isArmed, bool motorRunning) {
    RemainingTimeInput in;
    in.nowMs            = millis();
    in.armed            = isArmed;
    in.motorRunning     = motorRunning;
    in.currentAvailable = isCurrentAvailable();
    in.currentMa        = in.currentAvailable ? telemetry.getBatteryCurrentMilliAmps() : 0;
    in.ccRemainingMah   = batteryMonitor.getRemainingMah();
    in.voltageValid     = telemetry.isBatteryVoltageValid();
    in.voltageUsableMah = in.voltageValid
        ? batteryMonitor.usableMahFromVoltage(telemetry.getBatteryVoltageMilliVolts())
        : 0;

    // Non-zero only on the disarm edge of a flight that qualifies, so NVS is
    // written at most once per landing. The average is deliberately not
    // reseeded: the next takeoff continues from this flight's regime.
    const uint16_t learned = logic_.tick(in);
    if (learned != 0) {
        settings.setReferenceCurrentA(learned);
        settings.save();
    }
}

void RemainingTime::setReferenceCurrentA(uint16_t amps) {
    logic_.seed(amps);
}
