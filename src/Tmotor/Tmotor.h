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
 * - Enable Reporting command to activate ESC status messages
 *
 * Protocol details:
 * - Uses standard DroneCAN message IDs (1030, 1033, 1034, 1039, 1154, 1332)
 * - Generic Instruction (1000): Enable Reporting command (Internal Protocol Message ID 4670)
 * - ESC_STATUS (1034): ESC telemetry including ESC temperature
 * - Status 5 (1154): Motor temperature, capacitor temperature, and busbar current (single-frame, 10 Hz)
 * - PUSHCAN (1039): Additional telemetry (deprecated for motor temperature - use Status 5 instead)
 * - RawCommand (1030): Throttle control
 * - ParamCfg (1033): Deprecated - use Enable Reporting (1000) instead
 */
class Tmotor
{
public:
    Tmotor();

    // Main parsing method for ESC messages
    void parseEscMessage(twai_message_t *canMsg);

    // ESC control methods
    void setRawThrottle(int16_t throttle);
    void handle();

    // Initialization and configuration methods
    void requestNodeInfo();  // Request GetNodeInfo to discover ESC
    void requestParam(uint16_t paramIndex);  // Request parameter value
    void sendParamCfg(uint8_t escNodeId, uint16_t feedbackRate = 50, bool savePermanent = true);  // Send ParamCfg to configure ESC telemetry rate
    void sendEnableReporting(bool enable);  // Send Enable Reporting command (Message ID 1000) to enable/disable ESC status reporting

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
    bool isReady();  // Checks if ESC is available on the network (detected via NodeStatus)
    bool hasTelemetry();  // Checks if telemetry data is available

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
    unsigned long lastPushSci;
    unsigned long lastThrottleSend;

    // Enable Reporting state
    bool enableReportingSent;  // Track if Enable Reporting command was successfully sent

    // Transfer control
    uint8_t transferId;

    // Multi-frame transfer buffers and state
    uint8_t escStatusBuffer[64];  // Buffer for accumulating ESC_STATUS frames
    uint8_t escStatusBufferLen;   // Current length of accumulated data
    uint8_t escStatusTransferId;   // Transfer ID for current ESC_STATUS transfer
    bool escStatusTransferActive;  // Is ESC_STATUS transfer in progress?
    bool escStatusLastToggle;      // Last toggle bit value for ESC_STATUS

    uint8_t pushCanBuffer[64];     // Buffer for accumulating PUSHCAN frames
    uint8_t pushCanBufferLen;      // Current length of accumulated data
    uint8_t pushCanTransferId;     // Transfer ID for current PUSHCAN transfer
    bool pushCanTransferActive;    // Is PUSHCAN transfer in progress?
    bool pushCanLastToggle;        // Last toggle bit value for PUSHCAN

    // CAN bus constants
    const uint8_t escThrottleId = 0x00;  // Default ESC index for throttle

    // T-Motor protocol constants (DroneCAN message IDs)
    const uint16_t rawCommandDataTypeId = 1030;      // RawCommand
    const uint16_t paramCfgDataTypeId = 1033;        // ParamCfg
    const uint16_t escStatusDataTypeId = 1034;        // ESC_STATUS
    const uint16_t escStatus5DataTypeId = 1154;      // Status 5 (motor temperature)
    const uint16_t pushSciDataTypeId = 1038;           // PUSHSCI
    const uint16_t pushCanDataTypeId = 1039;         // PUSHCAN
    const uint16_t paramGetDataTypeId = 1332;         // ParamGet
    const uint16_t genericInstructionDataTypeId = 1000;  // Generic Instruction (Enable Reporting)

    // T-Motor-specific parsing methods
    void handleEscStatus(twai_message_t *canMsg);
    void handleEscStatus5(twai_message_t *canMsg);  // Handle Status 5 (Message ID 1154) for motor temperature
    void handlePushCan(twai_message_t *canMsg);

    // Float16 conversion methods
    float convertFloat16ToFloat(uint16_t value);
    uint16_t convertFloatToFloat16(float value);

    // Note: CAN utility functions (getTailByteFromPayload, isStartOfFrame, etc.)
    // are now in CanUtils class
};

#endif
