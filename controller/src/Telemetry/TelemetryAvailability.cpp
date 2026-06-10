#include "TelemetryAvailability.h"
#include "../BoardConfig.h"
#include "Telemetry.h"
#include "../config.h"

bool isCurrentAvailable(void) {
    return (getBoardConfig().hasCurrentSensor && telemetry.hasData()) || bluetoothBms.hasData();
}

bool isRpmAvailable(void) {
    return getBoardConfig().hasCurrentSensor && telemetry.hasData();
}

bool isPowerKwAvailable(void) {
    return isCurrentAvailable();
}

bool isBmsDataAvailable(void) {
    return bluetoothBms.hasData();
}

bool isBmsCellDataAvailable(void) {
    return bluetoothBms.hasCellData();
}
