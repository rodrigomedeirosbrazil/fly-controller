#include <Arduino.h>
#include <driver/twai.h>
#include <string.h>

#include "../config.h"
#include "TmotorCan.h"
#include "../Canbus/CanUtils.h"
#include "../Throttle/Throttle.h"

extern twai_message_t canMsg;

namespace {
// Static payload parser helpers for T-Motor CAN messages
static uint16_t parseUint16LE(const uint8_t *payload, uint8_t offset) {
    return (uint16_t)payload[offset] | ((uint16_t)payload[offset + 1] << 8);
}

static uint32_t parseUint32LE(const uint8_t *payload, uint8_t offset) {
    return (uint32_t)payload[offset] |
           ((uint32_t)payload[offset + 1] << 8) |
           ((uint32_t)payload[offset + 2] << 16) |
           ((uint32_t)payload[offset + 3] << 24);
}

static int16_t parseInt16LE(const uint8_t *payload, uint8_t offset) {
    return (int16_t)parseUint16LE(payload, offset);
}

static int32_t parseInt32LE(const uint8_t *payload, uint8_t offset) {
    return (int32_t)parseUint32LE(payload, offset);
}
} // namespace

// CRC16-CCITT-FALSE para DroneCAN/UAVCAN
// Based on TM-UAVCAN v2.3 manual, Appendix 5.1
static uint16_t crcAddByte(uint16_t crc_val, uint8_t byte) {
    crc_val ^= (uint16_t)((uint16_t)(byte) << 8);
    for (uint8_t j = 0; j < 8; j++) {
        if (crc_val & 0x8000U) {
            crc_val = (uint16_t)((uint16_t)(crc_val << 1) ^ 0x1021U);
        } else {
            crc_val = (uint16_t)(crc_val << 1);
        }
    }
    return crc_val;
}

static uint16_t crcAddSignature(uint16_t crc_val, uint64_t data_type_signature) {
    for (uint16_t shift_val = 0; shift_val < 64; shift_val = (uint16_t)(shift_val + 8U)) {
        crc_val = crcAddByte(crc_val, (uint8_t)(data_type_signature >> shift_val));
    }
    return crc_val;
}

static uint16_t crcAdd(uint16_t crc_val, const uint8_t* bytes, size_t len) {
    while (len--) {
        crc_val = crcAddByte(crc_val, *bytes++);
    }
    return crc_val;
}

TmotorCan::TmotorCan() {
    lastReadEscStatus = 0;
    lastReadPushCan = 0;
    lastMotorTempFromCanMs = 0;
    lastPushSci = 0;
    lastThrottleSend = 0;
    escDetectedAtMs = 0;
    enableReportingSent = false;
    transferId = 0;

    escTemperature = 0;
    motorTemperature = 0;
    batteryCurrentMilliAmps = 0;
    batteryVoltageMilliVolts = 0;
    rpm = 0;
    errorCount = 0;
    powerRatingPct = 0;
    escIndex = 0;

    // Initialize multi-frame transfer state
    escStatusBufferLen = 0;
    escStatusTransferId = 0xFF;  // Invalid ID
    escStatusTransferActive = false;
    escStatusLastToggle = false;

    pushCanBufferLen = 0;
    pushCanTransferId = 0xFF;  // Invalid ID
    pushCanTransferActive = false;
    pushCanLastToggle = false;
}

void TmotorCan::parseEscMessage(twai_message_t *canMsg) {
    // Extract DataType ID from CAN ID (assuming DroneCAN format)
    uint16_t dataTypeId = CanUtils::getDataTypeIdFromCanId(canMsg->identifier);
    // Route to appropriate handler
    if (dataTypeId == escStatusDataTypeId) {
        handleEscStatus(canMsg);
        return;
    }

    if (dataTypeId == escStatus5DataTypeId) {  // 1154
        DEBUG_PRINTLN("[Tmotor] -> Processing ESC_STATUS 5");
        handleEscStatus5(canMsg);
        return;
    }

    if (dataTypeId == pushCanDataTypeId) {
        DEBUG_PRINT("[PUSHCAN] Frame received - DLC=");
        DEBUG_PRINT(canMsg->data_length_code);
        DEBUG_PRINT(" Data=[");
        for (uint8_t i = 0; i < canMsg->data_length_code && i < 8; i++) {
            if (i > 0) { DEBUG_PRINT(" "); }
            if (canMsg->data[i] < 0x10) { DEBUG_PRINT("0"); }
            DEBUG_PRINT_HEX(canMsg->data[i], HEX);
        }
        DEBUG_PRINTLN("]");
        handlePushCan(canMsg);
        DEBUG_PRINT("[PUSHCAN] T_Motor=");
        DEBUG_PRINT(motorTemperature);
        DEBUG_PRINTLN("°C");
        return;
    }
}

void TmotorCan::handle() {
    extern Canbus canbus;
    if (canbus.getEscNodeId() == 0) {
        return;
    }

    // Track when ESC was first detected so we can delay the first SET attempt
    if (escDetectedAtMs == 0) {
        escDetectedAtMs = millis();
    }

    // Path B: replicate CloudLink's proprietary parameter SET to enable Status
    // upload (Status 1–5 reporting) persistently. Path A (Enable Reporting,
    // msg_id 0x123E) was tested and cannot override CloudLink's saved "Close"
    // — only the proprietary command on msg_id 0x1247 sticks.
    //
    // Sent once, 2 s after ESC detection. The setting persists in the ESC's
    // non-volatile memory, so this only needs to run once per ESC; we send it
    // every boot in case the user clicked "Close" via CloudLink in between.
    if (!enableReportingSent && millis() - escDetectedAtMs >= 2000) {
        sendStatusUploadSet(true);
        enableReportingSent = true;
    }

    // CAN throttle period: 2.5ms (400 Hz); allow some jitter
    if (millis() - lastThrottleSend < 2) {
        return;
    }

    setRawThrottle(0);
    lastThrottleSend = millis();
}

bool TmotorCan::hasTelemetry() {
    // Consider telemetry available if we've received ESC_STATUS within the last second
    return lastReadEscStatus != 0
        && millis() - lastReadEscStatus < 1000;
}

bool TmotorCan::hasRecentMotorTempFromCan() {
    return lastMotorTempFromCanMs != 0
        && millis() - lastMotorTempFromCanMs < 1000;
}

bool TmotorCan::isReady() {
    // ESC is ready if it has been detected via NodeStatus
    extern Canbus canbus;
    return canbus.getEscNodeId() != 0;
}

void TmotorCan::handleEscStatus(twai_message_t *canMsg) {
    uint8_t tailByte = CanUtils::getTailByteFromPayload(canMsg->data, canMsg->data_length_code);
    uint8_t frameTransferId = CanUtils::getTransferId(tailByte);
    bool isSOF = CanUtils::isStartOfFrame(tailByte);
    bool isEOF = CanUtils::isEndOfFrame(tailByte);
    bool isToggle = CanUtils::isToggleFrame(tailByte);

    // Handle multi-frame transfer
    if (isSOF) {
        // Start of new transfer - reset buffer
        escStatusBufferLen = 0;
        escStatusTransferId = frameTransferId;
        escStatusTransferActive = true;
        escStatusLastToggle = false;
    } else if (!escStatusTransferActive) {
        // Received non-SOF frame but no active transfer - ignore
        return;
    } else if (escStatusTransferId != frameTransferId) {
        // Transfer ID mismatch - reset
        escStatusBufferLen = 0;
        escStatusTransferActive = false;
        return;
    }

    // Check toggle bit for intermediate frames (should alternate)
    if (!isSOF && !isEOF) {
        if (isToggle == escStatusLastToggle) {
            escStatusBufferLen = 0;
            escStatusTransferActive = false;
            return;
        }
        escStatusLastToggle = isToggle;
    }

    // Extract payload data (excluding tail byte)
    uint8_t payloadLen = canMsg->data_length_code - 1;  // Exclude tail byte
    if (escStatusBufferLen + payloadLen > sizeof(escStatusBuffer)) {
        escStatusBufferLen = 0;
        escStatusTransferActive = false;
        return;
    }

    // Copy payload to buffer
    memcpy(escStatusBuffer + escStatusBufferLen, canMsg->data, payloadLen);
    escStatusBufferLen += payloadLen;

    // If this is EOF, process the complete message
    if (isEOF) {
        escStatusTransferActive = false;

        // Minimum payload size validation
        if (escStatusBufferLen < ESC_STATUS_MIN_PAYLOAD_SIZE) {
            escStatusBufferLen = 0;
            return;
        }

        // Extract error_count (uint32, little-endian, bytes 0-3)
        errorCount = parseUint32LE(escStatusBuffer, ESC_STATUS_ERROR_COUNT_OFFSET);

        // Extract voltage (float16, bytes 6-7, little-endian)
        uint16_t voltageFloat16 = parseUint16LE(escStatusBuffer, ESC_STATUS_VOLTAGE_OFFSET);
        float voltage = convertFloat16ToFloat(voltageFloat16);
        batteryVoltageMilliVolts = (uint16_t)(voltage * 1000.0f + 0.5f);

        // Extract current (float16, bytes 8-9, little-endian)
        uint16_t currentFloat16 = parseUint16LE(escStatusBuffer, ESC_STATUS_CURRENT_OFFSET);
        float current = convertFloat16ToFloat(currentFloat16);
        batteryCurrentMilliAmps = (uint32_t)(current * 1000.0f + 0.5f);

        // Extract ESC temperature (float16, bytes 10-11, little-endian)
        if (escStatusBufferLen >= (ESC_STATUS_TEMP_OFFSET + 1)) {
            uint16_t tempFloat16 = parseUint16LE(escStatusBuffer, ESC_STATUS_TEMP_OFFSET);
            float tempKelvin = convertFloat16ToFloat(tempFloat16);
            escTemperature = (uint8_t)(tempKelvin - 273.15f + 0.5f);  // Convert Kelvin to Celsius
        } else {
            escTemperature = 0;
        }

        // Extract RPM (int18, bytes 12-14, signed 3-byte value, little-endian)
        if (escStatusBufferLen >= ESC_STATUS_MIN_SIZE_WITH_TEMP) {
            int32_t rpmRaw = parseInt32LE(escStatusBuffer, ESC_STATUS_RPM_OFFSET) & 0x3FFFF;  // Only 18 bits
            // Sign extend from 18 bits to 32 bits
            if (rpmRaw & 0x20000) {  // Check sign bit (bit 17)
                rpmRaw |= 0xFFFC0000;  // Sign extend
            }
            rpm = (uint16_t)abs(rpmRaw);
        } else {
            rpm = 0;
        }

        // Extract power_rating_pct and esc_index (bytes 13-14)
        if (escStatusBufferLen >= ESC_STATUS_MIN_PAYLOAD_SIZE) {
            powerRatingPct = escStatusBuffer[ESC_STATUS_POWER_RATING_OFFSET] & 0x7F;  // Lower 7 bits
            if (escStatusBufferLen >= ESC_STATUS_MIN_SIZE_WITH_TEMP) {
                escIndex = escStatusBuffer[ESC_STATUS_ESC_INDEX_OFFSET] & 0x1F;  // Lower 5 bits
            }
        }

        lastReadEscStatus = millis();
        escStatusBufferLen = 0;  // Reset for next transfer
    }
}

void TmotorCan::handlePushCan(twai_message_t *canMsg) {
    uint8_t tailByte = CanUtils::getTailByteFromPayload(canMsg->data, canMsg->data_length_code);
    uint8_t frameTransferId = CanUtils::getTransferId(tailByte);
    bool isSOF = CanUtils::isStartOfFrame(tailByte);
    bool isEOF = CanUtils::isEndOfFrame(tailByte);
    bool isToggle = CanUtils::isToggleFrame(tailByte);

    // Handle multi-frame transfer
    if (isSOF) {
        // Start of new transfer - reset buffer
        pushCanBufferLen = 0;
        pushCanTransferId = frameTransferId;
        pushCanTransferActive = true;
        pushCanLastToggle = false;
    } else if (!pushCanTransferActive) {
        // Received non-SOF frame but no active transfer - ignore
        return;
    } else if (pushCanTransferId != frameTransferId) {
        // Transfer ID mismatch - reset
        pushCanBufferLen = 0;
        pushCanTransferActive = false;
        return;
    }

    // Check toggle bit for intermediate frames (should alternate)
    if (!isSOF && !isEOF) {
        if (isToggle == pushCanLastToggle) {
            pushCanBufferLen = 0;
            pushCanTransferActive = false;
            return;
        }
        pushCanLastToggle = isToggle;
    }

    // Extract payload data (excluding tail byte)
    uint8_t payloadLen = canMsg->data_length_code - 1;  // Exclude tail byte
    if (pushCanBufferLen + payloadLen > sizeof(pushCanBuffer)) {
        pushCanBufferLen = 0;
        pushCanTransferActive = false;
        return;
    }

    // Copy payload to buffer
    memcpy(pushCanBuffer + pushCanBufferLen, canMsg->data, payloadLen);
    pushCanBufferLen += payloadLen;

    // If this is EOF, process the complete message
    if (isEOF) {
        pushCanTransferActive = false;

        // PUSHCAN structure (Based on Manual Page 12, bytes list, NO LENGTH BYTE):
        // uint32 data_sequence (bytes 0-3)
        // Raw data value starts at byte 4
        // Motor temperature at bytes 22-23 of buffer (4 + 18 offset in data array)

        // Minimum size validation
        if (pushCanBufferLen < PUSHCAN_MIN_PAYLOAD_SIZE) {
            pushCanBufferLen = 0;
            return;
        }

        // Extract motor temperature from offset 22-23
        uint16_t motorTempValue = parseUint16LE(pushCanBuffer, PUSHCAN_MOTOR_TEMP_OFFSET);

        // Temperature is in float16 format (in Kelvin)
        float tempKelvin = convertFloat16ToFloat(motorTempValue);
        motorTemperature = (uint8_t)(tempKelvin - 273.15f + 0.5f);  // Convert Kelvin to Celsius

        lastMotorTempFromCanMs = millis();
        lastReadPushCan = millis();
        pushCanBufferLen = 0;  // Reset for next transfer
    } else {
        DEBUG_PRINTLN();  // New line for intermediate frames
    }
}

void TmotorCan::handleEscStatus5(twai_message_t *canMsg) {
    // Status 5: Single-frame (7 bytes + tail)
    // Bytes 0-1: idc (int16_t) - Estimated busbar current (0.1A units)
    // Bytes 2-3: cap_temp (int16_t) - Capacitor temperature (0.1°C units)
    // Bytes 4-5: motor_temp (int16_t) - Motor temperature (0.1°C units)
    // Byte 6: reserved
    // Byte 7: tail

    uint8_t tailByte = CanUtils::getTailByteFromPayload(canMsg->data, canMsg->data_length_code);

    // Validate single-frame
    if (!CanUtils::isStartOfFrame(tailByte) || !CanUtils::isEndOfFrame(tailByte)) {
        DEBUG_PRINTLN("[Tmotor] ERROR: Status 5 isn't single-frame");
        return;
    }

    // Minimum size check: 7 bytes data + 1 tail = 8 bytes
    if (canMsg->data_length_code < STATUS5_FRAME_LENGTH) {
        DEBUG_PRINTLN("[Tmotor] ERROR: Status 5 frame too short");
        return;
    }

    int16_t idc = parseInt16LE(canMsg->data, STATUS5_IDC_OFFSET);
    int16_t cap_temp_raw = parseInt16LE(canMsg->data, STATUS5_CAP_TEMP_OFFSET);
    int16_t motor_temp_raw = parseInt16LE(canMsg->data, STATUS5_MOTOR_TEMP_OFFSET);
    (void)idc;  // only used in DEBUG_PRINT

    int16_t cap_temp_celsius = (cap_temp_raw >= 0) ? (cap_temp_raw + 5) / 10 : (cap_temp_raw - 5) / 10;
    (void)cap_temp_celsius;  // only used in DEBUG_PRINT
    int16_t motor_temp_celsius = (motor_temp_raw >= 0) ? (motor_temp_raw + 5) / 10 : (motor_temp_raw - 5) / 10;

    // Store (convert to uint8_t, with range check)
    if (motor_temp_celsius < 0) {
        this->motorTemperature = 0;
    } else if (motor_temp_celsius > 255) {
        this->motorTemperature = 255;
    } else {
        this->motorTemperature = (uint8_t)motor_temp_celsius;
    }

    DEBUG_PRINT("[Tmotor] Status5(1154): IDC=");
    DEBUG_PRINT(idc / 10);
    DEBUG_PRINT(".");
    DEBUG_PRINT(abs(idc % 10));
    DEBUG_PRINT("A Cap=");
    DEBUG_PRINT(cap_temp_celsius);
    DEBUG_PRINT("C Motor=");
    DEBUG_PRINT(motor_temp_celsius);
    DEBUG_PRINTLN("C");

    lastMotorTempFromCanMs = millis();
    lastReadPushCan = millis();
}


void TmotorCan::requestParam(uint16_t paramIndex) {
    // Placeholder — ParamGet structure needs to be defined per DroneCAN spec
    DEBUG_PRINT("[Tmotor] ParamGet request not yet implemented for param index ");
    DEBUG_PRINTLN(paramIndex);
}

void TmotorCan::sendParamCfg(uint8_t escNodeId, uint16_t feedbackRate, bool savePermanent) {

    // ParamCfg structure: 27 bytes total
    uint8_t payload[27];
    memset(payload, 0, sizeof(payload));

    // Fill payload structure (all little-endian, 0xFF = don't change)
    payload[0] = 0;  // esc_index (0 for broadcast)
    // Bytes 1-4: esc_uuid (0xFFFFFFFF for broadcast/unknown)
    payload[1] = 0xFF;
    payload[2] = 0xFF;
    payload[3] = 0xFF;
    payload[4] = 0xFF;
    // Bytes 5-22: All other params set to 0xFF (don't change)
    for (int i = 5; i < 23; i++) {
        payload[i] = 0xFF;
    }
    // Byte 23: esc_can_rate (0xFF to keep current)
    payload[23] = 0xFF;
    // Bytes 24-25: esc_fdb_rate (feedback rate 0-400 Hz)
    payload[24] = (uint8_t)(feedbackRate & 0xFF);
    payload[25] = (uint8_t)((feedbackRate >> 8) & 0xFF);
    // Byte 26: esc_save_option (0=temporary, 1=permanent)
    payload[26] = savePermanent ? 1 : 0;

    // Calculate CRC for multi-frame transfer
    uint16_t crc = 0xFFFF;
    crc = crcAddSignature(crc, PARAMCFG_SIGNATURE);
    crc = crcAdd(crc, payload, 27);

    // Build CAN ID for ParamCfg (1033)
    uint32_t dataTypeId = paramCfgDataTypeId;
    uint32_t priority = 0x18;  // LOW priority
    uint32_t service = 0;  // Message frame
    uint32_t srcNodeId = canbus.getNodeId();

    uint32_t canId = (priority << 24) |
                     (dataTypeId << 8) |
                     (service << 7) |
                     (srcNodeId & 0x7F);

    twai_message_t localCanMsg;
    localCanMsg.identifier = canId;
    localCanMsg.extd = 1;
    localCanMsg.rtr = 0;
    localCanMsg.ss = 0;
    localCanMsg.self = 0;
    localCanMsg.dlc_non_comp = 0;

    // Save current TransferID (MUST be same for all frames!)
    uint8_t currentTransferId = transferId;

    esp_err_t tx_result;

    // ===== MULTI-FRAME WITH CRC =====
    // Total: 27 bytes payload + 2 bytes CRC = 29 bytes
    // Frame 1: CRC (2 bytes) + payload[0-4] (5 bytes) = 7 bytes
    // Frame 2: payload[5-11] (7 bytes)
    // Frame 3: payload[12-18] (7 bytes)
    // Frame 4: payload[19-25] (7 bytes)
    // Frame 5: payload[26] (1 byte)

    // Frame 1: CRC + payload[0-4]
    localCanMsg.data[0] = (uint8_t)(crc & 0xFF);         // CRC low byte
    localCanMsg.data[1] = (uint8_t)((crc >> 8) & 0xFF);  // CRC high byte
    localCanMsg.data[2] = payload[0];  // esc_index
    localCanMsg.data[3] = payload[1];  // esc_uuid[0]
    localCanMsg.data[4] = payload[2];  // esc_uuid[1]
    localCanMsg.data[5] = payload[3];  // esc_uuid[2]
    localCanMsg.data[6] = payload[4];  // esc_uuid[3]
    localCanMsg.data[7] = 0x80 | (currentTransferId & 0x1F);  // SOF=1, EOF=0, Toggle=0
    localCanMsg.data_length_code = 8;

    tx_result = twai_transmit(&localCanMsg, pdMS_TO_TICKS(100));
    if (tx_result != ESP_OK) {
        return;
    }
    delay(2);

    // Frame 2: payload[5-11]
    localCanMsg.data[0] = payload[5];
    localCanMsg.data[1] = payload[6];
    localCanMsg.data[2] = payload[7];
    localCanMsg.data[3] = payload[8];
    localCanMsg.data[4] = payload[9];
    localCanMsg.data[5] = payload[10];
    localCanMsg.data[6] = payload[11];
    localCanMsg.data[7] = 0x20 | (currentTransferId & 0x1F);  // SOF=0, EOF=0, Toggle=1
    localCanMsg.data_length_code = 8;

    tx_result = twai_transmit(&localCanMsg, pdMS_TO_TICKS(100));
    if (tx_result != ESP_OK) {
        return;
    }
    delay(2);

    // Frame 3: payload[12-18]
    localCanMsg.data[0] = payload[12];
    localCanMsg.data[1] = payload[13];
    localCanMsg.data[2] = payload[14];
    localCanMsg.data[3] = payload[15];
    localCanMsg.data[4] = payload[16];
    localCanMsg.data[5] = payload[17];
    localCanMsg.data[6] = payload[18];
    localCanMsg.data[7] = 0x00 | (currentTransferId & 0x1F);  // SOF=0, EOF=0, Toggle=0
    localCanMsg.data_length_code = 8;

    tx_result = twai_transmit(&localCanMsg, pdMS_TO_TICKS(100));
    if (tx_result != ESP_OK) {
        return;
    }
    delay(2);

    // Frame 4: payload[19-25]
    localCanMsg.data[0] = payload[19];
    localCanMsg.data[1] = payload[20];
    localCanMsg.data[2] = payload[21];
    localCanMsg.data[3] = payload[22];
    localCanMsg.data[4] = payload[23];
    localCanMsg.data[5] = payload[24];  // esc_fdb_rate low
    localCanMsg.data[6] = payload[25];  // esc_fdb_rate high
    localCanMsg.data[7] = 0x20 | (currentTransferId & 0x1F);  // SOF=0, EOF=0, Toggle=1
    localCanMsg.data_length_code = 8;

    tx_result = twai_transmit(&localCanMsg, pdMS_TO_TICKS(100));
    if (tx_result != ESP_OK) {
        return;
    }
    delay(2);

    // Frame 5: payload[26] (LAST FRAME)
    localCanMsg.data[0] = payload[26];  // esc_save_option
    localCanMsg.data[1] = 0x00;  // Padding
    localCanMsg.data[2] = 0x00;
    localCanMsg.data[3] = 0x00;
    localCanMsg.data[4] = 0x00;
    localCanMsg.data[5] = 0x00;
    localCanMsg.data[6] = 0x00;
    localCanMsg.data[7] = 0x40 | (currentTransferId & 0x1F);  // SOF=0, EOF=1, Toggle=0
    localCanMsg.data_length_code = 8;

    tx_result = twai_transmit(&localCanMsg, pdMS_TO_TICKS(100));
    if (tx_result != ESP_OK) {
        return;
    }

    // Increment TransferID only AFTER all frames sent
    transferId = (transferId + 1) & TRANSFER_ID_MASK;
    DEBUG_PRINTLN();
}

void TmotorCan::sendGenericInstructionMultiframe(uint16_t internalMsgId,
                                                  const uint8_t* innerData,
                                                  size_t innerLen) {
    // Build the wrapped Generic Instruction payload:
    //   [0..1] magic (CD AB)
    //   [2..3] internal msg_id (little-endian)
    //   [4..5] inner length (little-endian)
    //   [6..]  inner data
    const size_t HEADER_LEN = 6;
    const size_t MAX_INNER = 64;
    if (innerLen > MAX_INNER) return;

    uint8_t wrapped[HEADER_LEN + MAX_INNER];
    wrapped[0] = 0xCD;
    wrapped[1] = 0xAB;
    wrapped[2] = (uint8_t)(internalMsgId & 0xFF);
    wrapped[3] = (uint8_t)((internalMsgId >> 8) & 0xFF);
    wrapped[4] = (uint8_t)(innerLen & 0xFF);
    wrapped[5] = (uint8_t)((innerLen >> 8) & 0xFF);
    memcpy(wrapped + HEADER_LEN, innerData, innerLen);
    const size_t totalLen = HEADER_LEN + innerLen;

    // Multi-frame DroneCAN CRC seeded with the Generic Instruction signature.
    uint16_t crc = 0xFFFF;
    crc = crcAddSignature(crc, 0x1362ab78cd12f03aULL);
    crc = crcAdd(crc, wrapped, totalLen);

    // DroneCAN message-frame CAN ID for DataTypeId 1000.
    uint32_t priority = 0x18;  // LOW
    uint32_t dataTypeId = genericInstructionDataTypeId;
    uint32_t srcNodeId = canbus.getNodeId();
    uint32_t canId = (priority << 24) | (dataTypeId << 8) | (srcNodeId & 0x7F);

    twai_message_t msg;
    msg.identifier = canId;
    msg.extd = 1;
    msg.rtr = 0;
    msg.ss = 0;
    msg.self = 0;
    msg.dlc_non_comp = 0;

    const uint8_t tid = transferId & TRANSFER_ID_MASK;

    // First frame: CRC (2) + up to 5 bytes of wrapped + tail.
    size_t pos = 0;
    bool fitsInOne = (totalLen <= 5);
    {
        size_t chunk = totalLen < 5 ? totalLen : 5;
        msg.data[0] = (uint8_t)(crc & 0xFF);
        msg.data[1] = (uint8_t)((crc >> 8) & 0xFF);
        for (size_t i = 0; i < chunk; i++) msg.data[2 + i] = wrapped[i];
        uint8_t tail = (uint8_t)((1U << 7) | tid);   // SOF=1
        if (fitsInOne) tail |= (uint8_t)(1U << 6);   // also EOF
        msg.data[2 + chunk] = tail;
        msg.data_length_code = (uint8_t)(2 + chunk + 1);
        if (twai_transmit(&msg, pdMS_TO_TICKS(100)) != ESP_OK) return;
        pos = chunk;
    }
    if (fitsInOne) {
        transferId = (transferId + 1) & TRANSFER_ID_MASK;
        return;
    }

    // Subsequent frames: up to 7 bytes payload + tail. Toggle bit alternates
    // starting at 1 for frame 2 (matches captured CloudLink pattern).
    bool toggle = true;
    while (pos < totalLen) {
        delay(1);
        size_t remaining = totalLen - pos;
        size_t chunk = remaining > 7 ? 7 : remaining;
        bool isLast = (chunk == remaining);
        for (size_t i = 0; i < chunk; i++) msg.data[i] = wrapped[pos + i];
        uint8_t tail = tid;
        if (isLast) tail |= (uint8_t)(1U << 6);
        if (toggle) tail |= (uint8_t)(1U << 5);
        msg.data[chunk] = tail;
        msg.data_length_code = (uint8_t)(chunk + 1);
        if (twai_transmit(&msg, pdMS_TO_TICKS(100)) != ESP_OK) return;
        pos += chunk;
        toggle = !toggle;
    }

    transferId = (transferId + 1) & TRANSFER_ID_MASK;
}

void TmotorCan::sendEnableReporting(bool enable) {
    Serial.print("[Tmotor] sendEnableReporting(");
    Serial.print(enable ? "true" : "false");
    Serial.println(") — Path A: Generic Instruction msg_id=0x123E");

    // ENA_ESC_STAT_REP_T (4 bytes): uint32_t enable. 0=OFF, 1=ON.
    uint32_t enableVal = enable ? 1U : 0U;
    uint8_t inner[4] = {
        (uint8_t)(enableVal & 0xFF),
        (uint8_t)((enableVal >> 8) & 0xFF),
        (uint8_t)((enableVal >> 16) & 0xFF),
        (uint8_t)((enableVal >> 24) & 0xFF),
    };
    sendGenericInstructionMultiframe(0x123E, inner, sizeof(inner));
}

void TmotorCan::sendStatusUploadSet(bool open) {
    Serial.print("[Tmotor] sendStatusUploadSet(");
    Serial.print(open ? "open" : "close");
    Serial.println(") — Path B: proprietary msg_id=0x1247, 3 transfers");

    // Captured verbatim from CloudLink. See docs/tmotor-can-protocol.md.
    // Transfer 1 (section 0x02 — begin write handshake; 12 bytes inner data).
    static const uint8_t T1_INNER[12] = {
        0x41, 0x00, 0x01, 0x00, 0x02, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    // Transfer 2 (section 0x04 — general ESC config: PWM range, voltage limits.
    // Replicating verbatim will overwrite any custom CloudLink config in this
    // section; users should set their config in CloudLink before relying on
    // this firmware path. 56 bytes inner data).
    static const uint8_t T2_INNER[56] = {
        0x41, 0x00, 0x01, 0x00, 0x04, 0x00, 0x30, 0x00,
        0x01, 0x00, 0x01, 0x00, 0x4C, 0x04, 0x00, 0x00,
        0x94, 0x07, 0x00, 0x00, 0x76, 0x02, 0xC8, 0x00,
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA0, 0x0F,
        0xA0, 0x0F, 0x32, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    // Transfer 3 (section 0x10 — Status upload toggle; 44 bytes inner data,
    // byte 28 toggles between 0x00 (Close) and 0x80 (Open)).
    uint8_t t3_inner[44] = {
        0x41, 0x00, 0x01, 0x00, 0x10, 0x00, 0x24, 0x00,
        0x7B, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x40, 0x42, 0x0F, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xFA, 0x00, 0x00, 0x00,
    };
    t3_inner[28] = open ? 0x80 : 0x00;

    sendGenericInstructionMultiframe(0x1247, T1_INNER, sizeof(T1_INNER));
    delay(10);
    sendGenericInstructionMultiframe(0x1247, T2_INNER, sizeof(T2_INNER));
    delay(10);
    sendGenericInstructionMultiframe(0x1247, t3_inner, sizeof(t3_inner));
}

// Float16 conversion functions (based on PDF section 5.2)

float TmotorCan::convertFloat16ToFloat(uint16_t value) {
    const union {
        uint32_t u;
        float f;
    } magic = { (254UL - 15UL) << 23U };
    const union {
        uint32_t u;
        float f;
    } was_inf_nan = { (127UL + 16UL) << 23U };
    union {
        uint32_t u;
        float f;
    } out;

    out.u = (value & 0x7FFFU) << 13U;
    out.f *= magic.f;
    if (out.f >= was_inf_nan.f) {
        out.u |= 255UL << 23U;
    }
    out.u |= (value & 0x8000UL) << 16U;
    return out.f;
}

uint16_t TmotorCan::convertFloatToFloat16(float value) {
    union {
        uint32_t u;
        float f;
    } in;
    in.f = value;

    const union {
        uint32_t u;
        float f;
    } f32inf = { 255UL << 23 };
    const union {
        uint32_t u;
        float f;
    } f16inf = { 31UL << 23 };
    const union {
        uint32_t u;
        float f;
    } magic = { 15UL << 23 };
    const uint32_t sign_mask = 0x80000000U;
    const uint32_t round_mask = ~0xFFU;

    uint32_t sign = in.u & sign_mask;
    in.u ^= sign;
    uint16_t out = 0;

    if (in.u >= f32inf.u) {
        out = (in.u > f32inf.u) ? (uint16_t)0x7FFU : (uint16_t)0x7C00U;
    } else {
        in.u &= round_mask;
        in.f *= magic.f;
        in.u -= round_mask;
        if (in.u > f16inf.u) {
            in.u = f16inf.u;
        }
        out = (uint16_t)(in.u >> 13U);
    }
    out |= (uint16_t)(sign >> 16U);
    return out;
}

void TmotorCan::setRawThrottle(int16_t throttle) {
    // RawCommand (1030) - Throttle control
    // Range: 0 to MAX_THROTTLE (0 = no throttle, 8191 = max throttle)

    // Validate throttle range
    if (throttle < 0) throttle = 0;
    if (throttle > MAX_THROTTLE) throttle = MAX_THROTTLE;

    twai_message_t localCanMsg;

    // Build CAN ID for RawCommand (1030)
    uint32_t priority = 0x10;  // MEDIUM priority
    uint32_t dataTypeId = rawCommandDataTypeId;
    uint32_t service = 0;
    uint32_t srcNodeId = canbus.getNodeId();

    uint32_t canId = (priority << 24) |
                     (dataTypeId << 8) |
                     (service << 7) |
                     (srcNodeId & 0x7F);

    localCanMsg.identifier = canId;
    localCanMsg.extd = 1;
    localCanMsg.rtr = 0;
    localCanMsg.ss = 0;
    localCanMsg.self = 0;
    localCanMsg.dlc_non_comp = 0;

    // For 1 ESC: throttle uses 14 bits (2 bytes when packed)
    // Pack 14-bit throttle into bytes 0-1
    localCanMsg.data[0] = (uint8_t)(throttle & THROTTLE_BYTE0_MASK);
    localCanMsg.data[1] = (uint8_t)((throttle >> 8) & THROTTLE_BYTE1_MASK);

    // Padding bytes
    localCanMsg.data[2] = 0x00;
    localCanMsg.data[3] = 0x00;
    localCanMsg.data[4] = 0x00;
    localCanMsg.data[5] = 0x00;
    localCanMsg.data[6] = 0x00;

    // Tail byte: Single Frame Transfer
    localCanMsg.data[7] = 0xC0 | (transferId & TRANSFER_ID_MASK);

    localCanMsg.data_length_code = CAN_PAYLOAD_SIZE;

    // Send
    twai_transmit(&localCanMsg, pdMS_TO_TICKS(100));

    transferId = (transferId + 1) & TRANSFER_ID_MASK;
}
