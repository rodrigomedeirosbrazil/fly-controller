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
class TmotorCan
{
public:
    TmotorCan();

    // Main parsing method for ESC messages
    void parseEscMessage(twai_message_t *canMsg);

    // ESC control methods
    void setRawThrottle(int16_t throttle);
    void handle();

    // Initialization and configuration methods
    void requestParam(uint16_t paramIndex);  // Request parameter value
    void sendParamCfg(uint8_t escNodeId, uint16_t feedbackRate = 50, bool savePermanent = true);  // Send ParamCfg to configure ESC telemetry rate
    void sendEnableReporting(bool enable);  // Send Enable Reporting command (Message ID 1000) to enable/disable ESC status reporting

    // Getters for ESC data
    uint16_t getRpm() { return rpm; }
    uint16_t getBatteryVoltageMilliVolts() { return batteryVoltageMilliVolts; }
    uint32_t getBatteryCurrent() { return batteryCurrentMilliAmps; }
    uint8_t getEscTemperature() { return escTemperature; }  // ESC temperature in Celsius
    uint8_t getMotorTemperature() { return motorTemperature; }  // Motor temperature in Celsius
    uint32_t getErrorCount() { return errorCount; }
    uint8_t getPowerRatingPct() { return powerRatingPct; }
    uint8_t getEscIndex() { return escIndex; }

    // Connectivity status
    bool isReady();  // Checks if ESC is available on the network (detected via NodeStatus)
    bool hasTelemetry();  // Checks if telemetry data is available
    /** True if motor temperature was parsed from Status 5 or PUSHCAN within the last second (independent of hasTelemetry). */
    bool hasRecentMotorTempFromCan();

private:
    // ESC-specific data
    uint8_t escTemperature;      // ESC temperature in Celsius
    uint8_t motorTemperature;    // Motor temperature in Celsius
    uint32_t batteryCurrentMilliAmps;  // Current in milliamperes (ex: 50000 = 50.000A, max 500A = 500000 mA)
    uint16_t batteryVoltageMilliVolts; // Voltage in millivolts (3 decimal places, ex: 44100 = 44.100V, max 60V = 60000 mV)
    uint16_t rpm;                // RPM
    uint32_t errorCount;         // Error count
    uint8_t powerRatingPct;      // Power rating percentage
    uint8_t escIndex;             // ESC index (for multi-ESC support)

    // Timestamps for connectivity control
    unsigned long lastReadEscStatus;
    unsigned long lastReadPushCan;
    unsigned long lastMotorTempFromCanMs;  // Status 5 or PUSHCAN motor temp payload
    unsigned long lastPushSci;
    unsigned long lastThrottleSend;

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

    // ESC_STATUS payload field offsets
    const uint8_t ESC_STATUS_ERROR_COUNT_OFFSET = 0;  // Bytes 0-3
    const uint8_t ESC_STATUS_VOLTAGE_OFFSET = 6;      // Bytes 6-7 (float16)
    const uint8_t ESC_STATUS_CURRENT_OFFSET = 8;      // Bytes 8-9 (float16)
    const uint8_t ESC_STATUS_TEMP_OFFSET = 10;        // Bytes 10-11 (float16)
    const uint8_t ESC_STATUS_RPM_OFFSET = 12;         // Bytes 12-14 (int18)
    const uint8_t ESC_STATUS_POWER_RATING_OFFSET = 13;
    const uint8_t ESC_STATUS_ESC_INDEX_OFFSET = 14;
    const uint8_t ESC_STATUS_MIN_PAYLOAD_SIZE = 14;   // Minimum size for valid ESC_STATUS
    const uint8_t ESC_STATUS_MIN_SIZE_WITH_TEMP = 15; // For RPM/index fields

    // PUSHCAN payload constants
    const uint8_t PUSHCAN_MOTOR_TEMP_OFFSET = 22;     // Motor temperature at bytes 22-23 (offset from buffer start)
    const uint8_t PUSHCAN_MIN_PAYLOAD_SIZE = 24;      // Minimum size to reach temperature field

    // Status 5 (1154) parsing constants
    const uint8_t STATUS5_FRAME_LENGTH = 8;           // Single frame: 7 bytes + 1 tail
    const uint8_t STATUS5_IDC_OFFSET = 0;             // Bytes 0-1 (int16, 0.1A units)
    const uint8_t STATUS5_CAP_TEMP_OFFSET = 2;        // Bytes 2-3 (int16, 0.1°C units)
    const uint8_t STATUS5_MOTOR_TEMP_OFFSET = 4;      // Bytes 4-5 (int16, 0.1°C units)

    // UAVCAN multi-frame transfer constants
    const uint8_t TRANSFER_ID_MASK = 0x1F;
    const uint8_t CAN_PAYLOAD_SIZE = 8;

    // ParamCfg signature constant
    const uint64_t PARAMCFG_SIGNATURE = 0x948F5E0B33E0EDEEULL;

    // Throttle constants
    const int16_t MAX_THROTTLE = 8191;
    const uint8_t THROTTLE_BYTE0_MASK = 0xFF;
    const uint8_t THROTTLE_BYTE1_MASK = 0x3F;

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
