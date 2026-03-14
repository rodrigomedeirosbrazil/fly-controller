#ifndef TELEMETRY_AVAILABILITY_H
#define TELEMETRY_AVAILABILITY_H

#include <stdbool.h>

/**
 * Centralized availability of telemetry fields.
 * Use these instead of scattering hasCurrentSensor && telemetry.hasData().
 * "Available" means the sensor/source exists and has provided data; 0 is a valid value when available.
 */
bool isCurrentAvailable(void);
bool isRpmAvailable(void);
bool isPowerKwAvailable(void);
bool isBmsDataAvailable(void);
bool isBmsCellDataAvailable(void);

#endif // TELEMETRY_AVAILABILITY_H
