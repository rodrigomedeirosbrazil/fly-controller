#ifndef XAG_PROVIDER_H
#define XAG_PROVIDER_H

#include "TelemetryProvider.h"

/**
 * XAG Motor Controller Telemetry Provider
 *
 * Reads battery voltage from ADC and ESC temperature from NTC sensor
 * No CAN bus communication required
 */
TelemetryProvider createXagProvider();

#endif // XAG_PROVIDER_H

