#ifndef Tmotor_h
#define Tmotor_h

#include <driver/twai.h>

/**
 * T-Motor ESC Communication Class
 *
 * This class implements TM-UAVCAN v2.3 communication protocol for T-Motor Electronic Speed Controllers (ESC).
 *
 * Developed by: Rodrigo Medeiros
 *
 * This implementation is based on:
 * - T-Motor UAVCAN Protocol Specification v2.3
 * - DroneCAN protocol specifications
 *
 * Key features:
 * - TM-UAVCAN message parsing and generation
 * - ESC telemetry reading (RPM, voltage, current, ESC temperature, motor temperature)
 * - ESC control (Raw throttle command)
 * - NodeStatus announcement
 * - Float16 data conversion
 *
 * Protocol details:
 * - Uses standard DroneCAN message IDs (1030, 1033, 1034, 1039, 1332)
 * - ESC_STATUS (1034): ESC telemetry including ESC temperature
 * - PUSHCAN (1039): Additional telemetry including motor temperature
 * - RawCommand (1030): Throttle control
 */
class Tmotor
{
public:
    Tmotor();

    // Main parsing method for ESC messages
    void parseEscMessage(twai_message_t *canMsg);

    // ESC control methods
    void announce();
    void setRawThrottle(int16_t throttle);

    // Getters for ESC data
    uint16_t getRpm() { return rpm; }
    uint16_t getBatteryVoltageMilliVolts() { return batteryVoltageMilliVolts; }
    uint8_t getBatteryCurrent() { return batteryCurrent; }
    uint8_t getEscTemperature() { return escTemperature; }  // ESC temperature in Celsius
    uint8_t getMotorTemperature() { return motorTemperature; }  // Motor temperature in Celsius
    uint32_t getErrorCount() { return errorCount; }
    uint8_t getPowerRatingPct() { return powerRatingPct; }
    uint8_t getEscIndex() { return escIndex; }

    // Connectivity status
    bool isReady();

private:
    // ESC-specific data
    uint8_t escTemperature;      // ESC temperature in Celsius
    uint8_t motorTemperature;    // Motor temperature in Celsius
    uint8_t batteryCurrent;  // Current in amperes (integers, ex: 50 = 50A, max 200A)
    uint16_t batteryVoltageMilliVolts; // Voltage in millivolts (3 decimal places, ex: 44100 = 44.100V, max 60V = 60000 mV)
    uint16_t rpm;                // RPM
    uint32_t errorCount;         // Error count
    uint8_t powerRatingPct;      // Power rating percentage
    uint8_t escIndex;             // ESC index (for multi-ESC support)

    // Timestamps for connectivity control
    unsigned long lastReadEscStatus;
    unsigned long lastReadPushCan;
    unsigned long lastAnnounce;

    // Transfer control
    uint8_t transferId;

    // CAN bus constants
    const uint8_t nodeId = 0x13;
    const uint8_t escThrottleId = 0x00;  // Default ESC index for throttle

    // T-Motor protocol constants (DroneCAN message IDs)
    const uint16_t rawCommandDataTypeId = 1030;      // RawCommand
    const uint16_t paramCfgDataTypeId = 1033;        // ParamCfg
    const uint16_t escStatusDataTypeId = 1034;        // ESC_STATUS
    const uint16_t pushCanDataTypeId = 1039;         // PUSHCAN
    const uint16_t paramGetDataTypeId = 1332;         // ParamGet
    const uint16_t nodeStatusDataTypeId = 341;        // NodeStatus

    // T-Motor-specific parsing methods
    void handleEscStatus(twai_message_t *canMsg);
    void handlePushCan(twai_message_t *canMsg);

    // Float16 conversion methods
    float convertFloat16ToFloat(uint16_t value);
    uint16_t convertFloatToFloat16(float value);

    // Note: CAN utility functions (getTailByteFromPayload, isStartOfFrame, etc.)
    // are now in CanUtils class
};

#endif
