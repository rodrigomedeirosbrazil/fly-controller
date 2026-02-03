#ifndef TELEMETRY_PROVIDER_H
#define TELEMETRY_PROVIDER_H

#include "TelemetryData.h"

// Forward declaration for twai_message_t (to avoid including driver/twai.h in XAG mode)
#include "../config_controller.h"
#if USES_CAN_BUS
#include <driver/twai.h>
#else
// Forward declaration for XAG mode (no CAN bus)
// Use struct forward declaration to avoid conflicts
struct twai_message_t;
#endif

/**
 * Lightweight telemetry provider interface using function pointers
 * Avoids virtual inheritance overhead while providing polymorphism
 */
struct TelemetryProvider {
    void (*update)(void);                    // Update telemetry data
    void (*init)(void);                       // Initialize provider
    TelemetryData* (*getData)(void);          // Get pointer to telemetry data
    void (*sendNodeStatus)(void);                   // Send NodeStatus message (for CAN controllers)
    void (*handleCanMessage)(twai_message_t*); // Handle CAN message (for CAN controllers)
    bool (*hasTelemetry)(void);                    // Check if telemetry data is available

    // Telemetry data instance (one per provider)
    TelemetryData data;
};

#endif // TELEMETRY_PROVIDER_H

