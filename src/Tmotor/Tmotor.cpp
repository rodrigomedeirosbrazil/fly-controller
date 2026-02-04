#include <Arduino.h>
#include <driver/twai.h>
#include <string.h>

#include "../config.h"
#include "Tmotor.h"
#include "../Canbus/CanUtils.h"
#include "../Throttle/Throttle.h"

extern twai_message_t canMsg;


// CRC16-CCITT-FALSE para DroneCAN/UAVCAN
// Baseado no manual TM-UAVCAN v2.3, Apêndice 5.1
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

Tmotor::Tmotor() {
    lastReadEscStatus = 0;
    lastReadPushCan = 0;
    lastPushSci = 0;
    lastThrottleSend = 0;
    transferId = 0;

    escTemperature = 0;
    motorTemperature = 0;
    batteryCurrent = 0;
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

void Tmotor::parseEscMessage(twai_message_t *canMsg) {
    // Extract DataType ID from CAN ID (assuming DroneCAN format)
    uint16_t dataTypeId = CanUtils::getDataTypeIdFromCanId(canMsg->identifier);
    uint8_t sourceNodeId = CanUtils::getNodeIdFromCanId(canMsg->identifier);

    // Route to appropriate handler
    if (dataTypeId == escStatusDataTypeId) {
        handleEscStatus(canMsg);
        return;
    }

    if (dataTypeId == escStatus5DataTypeId) {  // 1154
        Serial.println("[Tmotor] -> Processing ESC_STATUS 5");
        handleEscStatus5(canMsg);
        return;
    }

    if (dataTypeId == pushCanDataTypeId) {
        Serial.print("[PUSHCAN] Frame received - DLC=");
        Serial.print(canMsg->data_length_code);
        Serial.print(" Data=[");
        for (uint8_t i = 0; i < canMsg->data_length_code && i < 8; i++) {
            if (i > 0) Serial.print(" ");
            if (canMsg->data[i] < 0x10) Serial.print("0");
            Serial.print(canMsg->data[i], HEX);
        }
        Serial.println("]");
        handlePushCan(canMsg);
        Serial.print("[PUSHCAN] T_Motor=");
        Serial.print(motorTemperature);
        Serial.println("°C");
        return;
    }
}

void Tmotor::handle() {
    // Check if ESC is detected and Enable Reporting not sent yet
    extern Canbus canbus;
    if (canbus.getEscNodeId() == 0) {
        return;
    }

    // CAN throttle period: 2.5ms (400 Hz)
    // Check if 2ms has elapsed (allowing for some jitter)
    if (millis() - lastThrottleSend < 2) {
        return;
    }

    setRawThrottle(0);

    lastThrottleSend = millis();
}

bool Tmotor::hasTelemetry() {
    // Consider telemetry available if we've received ESC_STATUS within the last second
    return lastReadEscStatus != 0
        && millis() - lastReadEscStatus < 1000;
}

bool Tmotor::isReady() {
    // ESC is ready if it has been detected via NodeStatus
    extern Canbus canbus;
    return canbus.getEscNodeId() != 0;
}

void Tmotor::handleEscStatus(twai_message_t *canMsg) {
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

        // Minimum payload size: 14 bytes
        if (escStatusBufferLen < 14) {
            escStatusBufferLen = 0;
            return;
        }

        // Debug: Print accumulated buffer
        // Serial.print("[ESC] Buffer: [");
        // for (uint8_t i = 0; i < escStatusBufferLen && i < 16; i++) {
        //     if (i > 0) Serial.print(" ");
        //     if (escStatusBuffer[i] < 0x10) Serial.print("0");
        //     Serial.print(escStatusBuffer[i], HEX);
        // }
        // Serial.println("]");

        // Extract error_count (uint32, little-endian, bytes 0-3)
        errorCount = (uint32_t)escStatusBuffer[0] |
                     ((uint32_t)escStatusBuffer[1] << 8) |
                     ((uint32_t)escStatusBuffer[2] << 16) |
                     ((uint32_t)escStatusBuffer[3] << 24);

        // Extract voltage (float16, bytes 6-7, little-endian) - CORRECTED POSITION
        uint16_t voltageFloat16 = (uint16_t)escStatusBuffer[6] | ((uint16_t)escStatusBuffer[7] << 8);
        float voltage = convertFloat16ToFloat(voltageFloat16);
        batteryVoltageMilliVolts = (uint16_t)(voltage * 1000.0f + 0.5f);

        // Extract current (float16, bytes 8-9, little-endian) - CORRECTED POSITION
        uint16_t currentFloat16 = (uint16_t)escStatusBuffer[8] | ((uint16_t)escStatusBuffer[9] << 8);
        float current = convertFloat16ToFloat(currentFloat16);
        batteryCurrent = (uint8_t)(current + 0.5f);

        // Extract ESC temperature (float16, bytes 10-11, little-endian) - CORRECTED POSITION
        if (escStatusBufferLen >= 12) {
            uint16_t tempFloat16 = (uint16_t)escStatusBuffer[10] | ((uint16_t)escStatusBuffer[11] << 8);
            float tempKelvin = convertFloat16ToFloat(tempFloat16);
            escTemperature = (uint8_t)(tempKelvin - 273.15f + 0.5f);  // Convert Kelvin to Celsius
        } else {
            escTemperature = 0;
        }

        // Extract RPM (int18, bytes 12-14, signed 3-byte value, little-endian)
        if (escStatusBufferLen >= 15) {
            int32_t rpmRaw = (int32_t)escStatusBuffer[12] |
                             ((int32_t)escStatusBuffer[13] << 8) |
                             ((int32_t)(escStatusBuffer[14] & 0x03) << 16);  // Only lower 2 bits of byte 14
            // Sign extend from 18 bits to 32 bits
            if (rpmRaw & 0x20000) {  // Check sign bit (bit 17)
                rpmRaw |= 0xFFFC0000;  // Sign extend
            }
            rpm = (uint16_t)abs(rpmRaw);
        } else {
            rpm = 0;
        }

        // Extract power_rating_pct and esc_index (byte 13)
        if (escStatusBufferLen >= 14) {
            powerRatingPct = escStatusBuffer[13] & 0x7F;  // Lower 7 bits
            if (escStatusBufferLen >= 15) {
                escIndex = escStatusBuffer[14] & 0x1F;  // Lower 5 bits
            }
        }

        lastReadEscStatus = millis();
        escStatusBufferLen = 0;  // Reset for next transfer
    }
}

void Tmotor::handlePushCan(twai_message_t *canMsg) {
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
        //
        // Data Packet Structure (inside Raw data):
        // Offset 0-1: Frame header
        // Offset 2: Frame ID
        // ...
        // Offset 18-19: Temperature (2 bytes)
        //
        // Total offset in pushCanBuffer: 4 (sequence) + 18 = 22

        // Minimum size: 4 (data_sequence) + 20 (to reach bytes 19-20) = 24 bytes
        if (pushCanBufferLen < 24) {
            pushCanBufferLen = 0;
            return;
        }

        // Extract motor temperature from bytes 18-19 of data array
        // data array starts at byte 4 (after data_sequence)
        // So bytes 18-19 of data array are at indices 4+18=22 and 4+19=23
        uint16_t motorTempValue = (uint16_t)pushCanBuffer[22] | ((uint16_t)pushCanBuffer[23] << 8);

        // Assuming temperature is in float16 format (similar to ESC_STATUS)
        // If it's in Kelvin, convert to Celsius
        float tempKelvin = convertFloat16ToFloat(motorTempValue);
        motorTemperature = (uint8_t)(tempKelvin - 273.15f + 0.5f);  // Convert Kelvin to Celsius

        lastReadPushCan = millis();
        pushCanBufferLen = 0;  // Reset for next transfer
    } else {
        Serial.println();  // New line for intermediate frames
    }
}

void Tmotor::handleEscStatus5(twai_message_t *canMsg) {
    // Status 5: Single-frame (7 bytes + tail)
    // Bytes 0-1: idc (int16_t) - Estimated busbar current
    // Bytes 2-3: cap_temp (int16_t) - Capacitor temperature in 0.1°C
    // Bytes 4-5: motor_temp (int16_t) - Motor temperature in 0.1°C
    // Byte 6: reserved
    // Byte 7: tail

    uint8_t tailByte = CanUtils::getTailByteFromPayload(canMsg->data, canMsg->data_length_code);

    // Validar single-frame
    if (!CanUtils::isStartOfFrame(tailByte) || !CanUtils::isEndOfFrame(tailByte)) {
        Serial.println("[Tmotor] ERROR: Status 5 isn't single-frame");
        return;
    }

    // Minimum size check: 7 bytes data + 1 tail = 8 bytes
    if (canMsg->data_length_code < 8) {
        Serial.println("[Tmotor] ERROR: Status 5 frame too short");
        return;
    }

    int16_t idc = (int16_t)((uint16_t)canMsg->data[0] |
                            ((uint16_t)canMsg->data[1] << 8));

    int16_t cap_temp_raw = (int16_t)((uint16_t)canMsg->data[2] |
                                     ((uint16_t)canMsg->data[3] << 8));

    int16_t motor_temp_raw = (int16_t)((uint16_t)canMsg->data[4] |
                                       ((uint16_t)canMsg->data[5] << 8));

    // Convert to Celsius (comes in 0.1°C)
    float cap_temp_celsius = cap_temp_raw / 10.0f;
    float motor_temp_celsius = motor_temp_raw / 10.0f;

    // Store (convert to uint8_t, rounding)
    this->motorTemperature = (uint8_t)(motor_temp_celsius + 0.5f);

    Serial.print("[Tmotor] Status 5: IDC=");
    Serial.print(idc * 0.1f);
    Serial.print("A, Cap=");
    Serial.print(cap_temp_celsius);
    Serial.print("°C, Motor=");
    Serial.print(motor_temp_celsius);
    Serial.println("°C");

    lastReadPushCan = millis();
}


void Tmotor::requestParam(uint16_t paramIndex) {
    // Send ParamGet service request to read ESC parameter
    // Service ID for ParamGet is typically 10 (uavcan.protocol.param.GetSet)
    twai_message_t localCanMsg;

    // For now, this is a placeholder - ParamGet structure needs to be defined
    // based on the actual DroneCAN specification
    Serial.print("[Tmotor] ParamGet request not yet implemented for param index ");
    Serial.println(paramIndex);
}

void Tmotor::sendParamCfg(uint8_t escNodeId, uint16_t feedbackRate, bool savePermanent) {

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
    // ParamCfg signature: 0x948F5E0B33E0EDEE (from manual section 4.3.1)
    uint64_t signature = 0x948F5E0B33E0EDEEULL;

    uint16_t crc = 0xFFFF;
    crc = crcAddSignature(crc, signature);
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
    transferId = (transferId + 1) & 0x1F;
    Serial.println();
}

void Tmotor::sendEnableReporting(bool enable) {
    // Payload: Header (6 bytes) + Data (4 bytes) = 10 bytes
    uint8_t payload[10];

    // CAN_APP_PROTO_HEAD
    payload[0] = 0xCD;  // magic[0] (0xABCD little-endian)
    payload[1] = 0xAB;  // magic[1]
    payload[2] = 0x3E;  // msg_id[0] (4670 = 0x123E)
    payload[3] = 0x12;  // msg_id[1]
    payload[4] = 0x04;  // len[0] (4 bytes)
    payload[5] = 0x00;  // len[1]

    // ENA_ESC_STAT_REP_T
    uint32_t enable_val = enable ? 1 : 0;
    payload[6] = (uint8_t)(enable_val & 0xFF);
    payload[7] = (uint8_t)((enable_val >> 8) & 0xFF);
    payload[8] = (uint8_t)((enable_val >> 16) & 0xFF);
    payload[9] = (uint8_t)((enable_val >> 24) & 0xFF);

    // Calculate CRC
    uint64_t signature = 0x1362ab78cd12f03aULL;  // Generic instruction signature
    uint16_t crc = 0xFFFF;
    crc = crcAddSignature(crc, signature);
    crc = crcAdd(crc, payload, 10);

    // CAN ID para Generic Instruction (1000)
    uint32_t priority = 0x18;  // LOW
    uint32_t dataTypeId = genericInstructionDataTypeId;  // 1000
    uint32_t service = 0;
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

    uint8_t currentTransferId = transferId;

    // Frame 1: CRC (2) + payload[0-4] (5) = 7 bytes
    localCanMsg.data[0] = (uint8_t)(crc & 0xFF);
    localCanMsg.data[1] = (uint8_t)((crc >> 8) & 0xFF);
    localCanMsg.data[2] = payload[0];  // magic[0]
    localCanMsg.data[3] = payload[1];  // magic[1]
    localCanMsg.data[4] = payload[2];  // msg_id[0]
    localCanMsg.data[5] = payload[3];  // msg_id[1]
    localCanMsg.data[6] = payload[4];  // len[0]
    localCanMsg.data[7] = 0x80 | (currentTransferId & 0x1F);  // SOF=1, EOF=0, Toggle=0
    localCanMsg.data_length_code = 8;

    esp_err_t tx_result = twai_transmit(&localCanMsg, pdMS_TO_TICKS(100));
    if (tx_result != ESP_OK) {
        Serial.println("[ERROR] Frame 1 TX failed");
        return;
    }
    delay(2);

    // Frame 2: payload[5-9] (5 bytes) - ÚLTIMO FRAME
    localCanMsg.data[0] = payload[5];  // len[1]
    localCanMsg.data[1] = payload[6];  // enable[0]
    localCanMsg.data[2] = payload[7];  // enable[1]
    localCanMsg.data[3] = payload[8];  // enable[2]
    localCanMsg.data[4] = payload[9];  // enable[3]
    localCanMsg.data[5] = 0x00;  // padding
    localCanMsg.data[6] = 0x00;  // padding
    localCanMsg.data[7] = 0x40 | (currentTransferId & 0x1F);  // SOF=0, EOF=1, Toggle=0
    localCanMsg.data_length_code = 8;

    tx_result = twai_transmit(&localCanMsg, pdMS_TO_TICKS(100));
    if (tx_result != ESP_OK) {
        Serial.println("[ERROR] Frame 2 TX failed");
        return;
    }

    transferId = (transferId + 1) & 0x1F;
}

// Float16 conversion functions (based on PDF section 5.2)

float Tmotor::convertFloat16ToFloat(uint16_t value) {
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

uint16_t Tmotor::convertFloatToFloat16(float value) {
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

void Tmotor::setRawThrottle(int16_t throttle) {
    // RawCommand (1030) - Throttle control
    // Range: 0 to 8191 (0 = no throttle, 8191 = max throttle)

    // Validate throttle range
    if (throttle < 0) throttle = 0;
    if (throttle > 8191) throttle = 8191;

    twai_message_t localCanMsg;

    // Build CAN ID for RawCommand (1030)
    uint32_t priority = 0x10;  // MEDIUM priority
    uint32_t dataTypeId = rawCommandDataTypeId;  // 1030
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
    localCanMsg.data[0] = (uint8_t)(throttle & 0xFF);         // Lower 8 bits
    localCanMsg.data[1] = (uint8_t)((throttle >> 8) & 0x3F);  // Upper 6 bits

    // Padding bytes
    localCanMsg.data[2] = 0x00;
    localCanMsg.data[3] = 0x00;
    localCanMsg.data[4] = 0x00;
    localCanMsg.data[5] = 0x00;
    localCanMsg.data[6] = 0x00;

    // Tail byte: Single Frame Transfer
    localCanMsg.data[7] = 0xC0 | (transferId & 0x1F);

    localCanMsg.data_length_code = 8;

    // Send
    twai_transmit(&localCanMsg, pdMS_TO_TICKS(100));

    transferId = (transferId + 1) & 0x1F;
}
