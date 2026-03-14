#include "TelemetryAvailability.h"
#include "../BoardConfig.h"
#include "Telemetry.h"
#include "../JbdBms/JbdBms.h"

extern JbdBms jbdBms;

bool isCurrentAvailable(void) {
    return getBoardConfig().hasCurrentSensor && telemetry.hasData();
}

bool isRpmAvailable(void) {
    return getBoardConfig().hasCurrentSensor && telemetry.hasData();
}

bool isPowerKwAvailable(void) {
    return isCurrentAvailable();
}

bool isBmsDataAvailable(void) {
    return jbdBms.hasData();
}

bool isBmsCellDataAvailable(void) {
    return jbdBms.hasCellData();
}
