#include <Arduino.h>
#include <driver/twai.h>
#include <string.h>

#include "../config.h"
#include "Tmotor.h"
#include "../Canbus/CanUtils.h"

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
    lastAnnounce = millis();
    transferId = 0;

    escTemperature = 0;
    motorTemperature = 0;
    batteryCurrent = 0;
    batteryVoltageMilliVolts = 0;
    rpm = 0;
    errorCount = 0;
    powerRatingPct = 0;
    escIndex = 0;

    detectedEscNodeId = 0;
    paramCfgSent = false;
    lastParamCfgSent = 0;

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

    Serial.print("[Tmotor] parseEscMessage: DataTypeID=");
    Serial.print(dataTypeId);
    Serial.print(" (expected: ESC_STATUS=1034, PUSHCAN=1039)");
    Serial.print(" CAN ID=0x");
    Serial.print(canMsg->identifier, HEX);
    Serial.print(" DLC=");
    Serial.println(canMsg->data_length_code);

    // Route to appropriate handler
    if (dataTypeId == escStatusDataTypeId) {
        Serial.println("[Tmotor] -> Processing ESC_STATUS");
        handleEscStatus(canMsg);
        Serial.print("[Tmotor] ESC_STATUS parsed: RPM=");
        Serial.print(rpm);
        Serial.print(" V=");
        Serial.print(batteryVoltageMilliVolts);
        Serial.print("mV I=");
        Serial.print(batteryCurrent);
        Serial.print("A T_ESC=");
        Serial.print(escTemperature);
        Serial.println("°C");
        return;
    }

    if (dataTypeId == pushCanDataTypeId) {
        Serial.println("[Tmotor] -> Processing PUSHCAN");
        handlePushCan(canMsg);
        Serial.print("[Tmotor] PUSHCAN parsed: T_Motor=");
        Serial.print(motorTemperature);
        Serial.println("°C");
        return;
    }

    Serial.print("[Tmotor] -> Unknown Data Type ID ");
    Serial.println(dataTypeId);
}

bool Tmotor::isReady() {
    // Consider ready if we've received ESC_STATUS within the last second
    return lastReadEscStatus != 0
        && millis() - lastReadEscStatus < 1000;
}

void Tmotor::handleEscStatus(twai_message_t *canMsg) {
    uint8_t tailByte = CanUtils::getTailByteFromPayload(canMsg->data, canMsg->data_length_code);
    uint8_t frameTransferId = CanUtils::getTransferId(tailByte);
    bool isSOF = CanUtils::isStartOfFrame(tailByte);
    bool isEOF = CanUtils::isEndOfFrame(tailByte);
    bool isToggle = CanUtils::isToggleFrame(tailByte);

    Serial.print("[Tmotor] handleEscStatus: DLC=");
    Serial.print(canMsg->data_length_code);
    Serial.print(" TID=");
    Serial.print(frameTransferId);
    Serial.print(" SOF=");
    Serial.print(isSOF ? "1" : "0");
    Serial.print(" EOF=");
    Serial.print(isEOF ? "1" : "0");
    Serial.print(" Toggle=");
    Serial.print(isToggle ? "1" : "0");

    // Handle multi-frame transfer
    if (isSOF) {
        // Start of new transfer - reset buffer
        escStatusBufferLen = 0;
        escStatusTransferId = frameTransferId;
        escStatusTransferActive = true;
        escStatusLastToggle = false;
        Serial.print(" [NEW TRANSFER]");
    } else if (!escStatusTransferActive) {
        // Received non-SOF frame but no active transfer - ignore
        Serial.println(" [IGNORED: No active transfer]");
        return;
    } else if (escStatusTransferId != frameTransferId) {
        // Transfer ID mismatch - reset
        Serial.print(" [RESET: TID mismatch, expected ");
        Serial.print(escStatusTransferId);
        Serial.print(" got ");
        Serial.print(frameTransferId);
        Serial.println("]");
        escStatusBufferLen = 0;
        escStatusTransferActive = false;
        return;
    }

    // Check toggle bit for intermediate frames (should alternate)
    if (!isSOF && !isEOF) {
        if (isToggle == escStatusLastToggle) {
            Serial.println(" [ERROR: Toggle bit mismatch]");
            escStatusBufferLen = 0;
            escStatusTransferActive = false;
            return;
        }
        escStatusLastToggle = isToggle;
    }

    // Extract payload data (excluding tail byte)
    uint8_t payloadLen = canMsg->data_length_code - 1;  // Exclude tail byte
    if (escStatusBufferLen + payloadLen > sizeof(escStatusBuffer)) {
        Serial.println(" [ERROR: Buffer overflow]");
        escStatusBufferLen = 0;
        escStatusTransferActive = false;
        return;
    }

    // Copy payload to buffer
    memcpy(escStatusBuffer + escStatusBufferLen, canMsg->data, payloadLen);
    escStatusBufferLen += payloadLen;

    Serial.print(" [Accumulated: ");
    Serial.print(escStatusBufferLen);
    Serial.print(" bytes]");

    // If this is EOF, process the complete message
    if (isEOF) {
        Serial.println(" [COMPLETE]");
        escStatusTransferActive = false;

        // Minimum payload size: 4 (error_count) + 2 (voltage) + 2 (current) + 2 (temp) + 3 (rpm) + 1 (power/index) = 14 bytes
        if (escStatusBufferLen < 14) {
            Serial.print("[Tmotor] ERROR: Payload too small: ");
            Serial.print(escStatusBufferLen);
            Serial.println(" bytes (expected >= 14)");
            escStatusBufferLen = 0;
            return;
        }

        // Debug: Print accumulated buffer
        Serial.print("[Tmotor] Buffer: [");
        for (uint8_t i = 0; i < escStatusBufferLen && i < 16; i++) {
            if (i > 0) Serial.print(" ");
            if (escStatusBuffer[i] < 0x10) Serial.print("0");
            Serial.print(escStatusBuffer[i], HEX);
        }
        Serial.println("]");

        // Extract error_count (uint32, little-endian)
        errorCount = (uint32_t)escStatusBuffer[0] |
                     ((uint32_t)escStatusBuffer[1] << 8) |
                     ((uint32_t)escStatusBuffer[2] << 16) |
                     ((uint32_t)escStatusBuffer[3] << 24);

        // Extract voltage (float16, bytes 4-5)
        uint16_t voltageFloat16 = (uint16_t)escStatusBuffer[4] | ((uint16_t)escStatusBuffer[5] << 8);
        Serial.print("[Tmotor] Voltage float16: 0x");
        Serial.print(voltageFloat16, HEX);
        float voltage = convertFloat16ToFloat(voltageFloat16);
        Serial.print(" -> ");
        Serial.print(voltage, 3);
        Serial.println("V");
        batteryVoltageMilliVolts = (uint16_t)(voltage * 1000.0f + 0.5f);

        // Extract current (float16, bytes 6-7)
        uint16_t currentFloat16 = (uint16_t)escStatusBuffer[6] | ((uint16_t)escStatusBuffer[7] << 8);
        Serial.print("[Tmotor] Current float16: 0x");
        Serial.print(currentFloat16, HEX);
        float current = convertFloat16ToFloat(currentFloat16);
        Serial.print(" -> ");
        Serial.print(current, 3);
        Serial.println("A");
        batteryCurrent = (uint8_t)(current + 0.5f);

        // Extract ESC temperature (float16, bytes 8-9) - in Kelvin
        uint16_t tempFloat16 = (uint16_t)escStatusBuffer[8] | ((uint16_t)escStatusBuffer[9] << 8);
        Serial.print("[Tmotor] Temp float16: 0x");
        Serial.print(tempFloat16, HEX);
        float tempKelvin = convertFloat16ToFloat(tempFloat16);
        Serial.print(" -> ");
        Serial.print(tempKelvin, 2);
        Serial.print("K (");
        Serial.print(tempKelvin - 273.15f, 2);
        Serial.println("°C)");
        escTemperature = (uint8_t)(tempKelvin - 273.15f + 0.5f);  // Convert Kelvin to Celsius

        // Extract RPM (int18, bytes 10-12, signed 3-byte value)
        int32_t rpmRaw = (int32_t)escStatusBuffer[10] |
                         ((int32_t)escStatusBuffer[11] << 8) |
                         ((int32_t)escStatusBuffer[12] << 16);
        // Sign extend from 18 bits to 32 bits
        if (rpmRaw & 0x20000) {  // Check sign bit (bit 17)
            rpmRaw |= 0xFFFC0000;  // Sign extend
        }
        rpm = (uint16_t)abs(rpmRaw);

        // Extract power_rating_pct and esc_index (byte 13)
        if (escStatusBufferLen >= 14) {
            powerRatingPct = escStatusBuffer[13] & 0x7F;  // Lower 7 bits
            if (escStatusBufferLen >= 15) {
                escIndex = escStatusBuffer[14] & 0x1F;  // Lower 5 bits
            }
        }

        Serial.print("[Tmotor] ESC_STATUS parsed: RPM=");
        Serial.print(rpm);
        Serial.print(" V=");
        Serial.print(batteryVoltageMilliVolts);
        Serial.print("mV I=");
        Serial.print(batteryCurrent);
        Serial.print("A T_ESC=");
        Serial.print(escTemperature);
        Serial.println("°C");

        lastReadEscStatus = millis();
        escStatusBufferLen = 0;  // Reset for next transfer
    } else {
        Serial.println();  // New line for intermediate frames
    }
}

void Tmotor::handlePushCan(twai_message_t *canMsg) {
    uint8_t tailByte = CanUtils::getTailByteFromPayload(canMsg->data, canMsg->data_length_code);
    uint8_t frameTransferId = CanUtils::getTransferId(tailByte);
    bool isSOF = CanUtils::isStartOfFrame(tailByte);
    bool isEOF = CanUtils::isEndOfFrame(tailByte);
    bool isToggle = CanUtils::isToggleFrame(tailByte);

    Serial.print("[Tmotor] handlePushCan: DLC=");
    Serial.print(canMsg->data_length_code);
    Serial.print(" TID=");
    Serial.print(frameTransferId);
    Serial.print(" SOF=");
    Serial.print(isSOF ? "1" : "0");
    Serial.print(" EOF=");
    Serial.print(isEOF ? "1" : "0");
    Serial.print(" Toggle=");
    Serial.print(isToggle ? "1" : "0");

    // Handle multi-frame transfer
    if (isSOF) {
        // Start of new transfer - reset buffer
        pushCanBufferLen = 0;
        pushCanTransferId = frameTransferId;
        pushCanTransferActive = true;
        pushCanLastToggle = false;
        Serial.print(" [NEW TRANSFER]");
    } else if (!pushCanTransferActive) {
        // Received non-SOF frame but no active transfer - ignore
        Serial.println(" [IGNORED: No active transfer]");
        return;
    } else if (pushCanTransferId != frameTransferId) {
        // Transfer ID mismatch - reset
        Serial.print(" [RESET: TID mismatch, expected ");
        Serial.print(pushCanTransferId);
        Serial.print(" got ");
        Serial.print(frameTransferId);
        Serial.println("]");
        pushCanBufferLen = 0;
        pushCanTransferActive = false;
        return;
    }

    // Check toggle bit for intermediate frames (should alternate)
    if (!isSOF && !isEOF) {
        if (isToggle == pushCanLastToggle) {
            Serial.println(" [ERROR: Toggle bit mismatch]");
            pushCanBufferLen = 0;
            pushCanTransferActive = false;
            return;
        }
        pushCanLastToggle = isToggle;
    }

    // Extract payload data (excluding tail byte)
    uint8_t payloadLen = canMsg->data_length_code - 1;  // Exclude tail byte
    if (pushCanBufferLen + payloadLen > sizeof(pushCanBuffer)) {
        Serial.println(" [ERROR: Buffer overflow]");
        pushCanBufferLen = 0;
        pushCanTransferActive = false;
        return;
    }

    // Copy payload to buffer
    memcpy(pushCanBuffer + pushCanBufferLen, canMsg->data, payloadLen);
    pushCanBufferLen += payloadLen;

    Serial.print(" [Accumulated: ");
    Serial.print(pushCanBufferLen);
    Serial.print(" bytes]");

    // If this is EOF, process the complete message
    if (isEOF) {
        Serial.println(" [COMPLETE]");
        pushCanTransferActive = false;

        // PUSHCAN structure:
        // uint32 data_sequence (bytes 0-3)
        // struct { uint8_t len; uint8_t data[255]; }data;
        // The motor temperature is in bytes 19-20 of the data array

        // Minimum size: 4 (data_sequence) + 1 (len) + 20 (to reach bytes 19-20) = 25 bytes
        if (pushCanBufferLen < 25) {
            Serial.print("[Tmotor] ERROR: Payload too small: ");
            Serial.print(pushCanBufferLen);
            Serial.println(" bytes (expected >= 25)");
            pushCanBufferLen = 0;
            return;
        }

        // Extract data length (byte 4, after data_sequence)
        uint8_t dataLen = pushCanBuffer[4];

        // Check if we have enough data to read bytes 19-20
        if (dataLen < 21) {
            Serial.print("[Tmotor] ERROR: Data length too small: ");
            Serial.print(dataLen);
            Serial.println(" (expected >= 21)");
            pushCanBufferLen = 0;
            return;
        }

        // Extract motor temperature from bytes 19-20 of data array
        // data array starts at byte 5 (after data_sequence and len)
        // So bytes 19-20 of data array are at indices 5+19=24 and 5+20=25
        uint16_t motorTempValue = (uint16_t)pushCanBuffer[24] | ((uint16_t)pushCanBuffer[25] << 8);

        // Assuming temperature is in float16 format (similar to ESC_STATUS)
        // If it's in Kelvin, convert to Celsius
        float tempKelvin = convertFloat16ToFloat(motorTempValue);
        motorTemperature = (uint8_t)(tempKelvin - 273.15f + 0.5f);  // Convert Kelvin to Celsius

        Serial.print("[Tmotor] PUSHCAN parsed: T_Motor=");
        Serial.print(motorTemperature);
        Serial.println("°C");

        lastReadPushCan = millis();
        pushCanBufferLen = 0;  // Reset for next transfer
    } else {
        Serial.println();  // New line for intermediate frames
    }
}

void Tmotor::announce() {
    if (millis() - lastAnnounce < 1000) {
        return;
    }

    lastAnnounce = millis();

    uint32_t uptimeSec = millis() / 1000;

    twai_message_t localCanMsg;
    memset(&localCanMsg, 0, sizeof(twai_message_t));  // Initialize all fields to zero

    uint32_t dataTypeId = nodeStatusDataTypeId;

    uint32_t canId = 0;
    canId |= ((uint32_t)0 << 26);              // Priority
    canId |= ((uint32_t)dataTypeId << 8);      // DataType ID
    canId |= (uint32_t)(nodeId & 0xFF);        // Source node ID
    localCanMsg.identifier = canId;
    localCanMsg.extd = 1;  // Extended frame (29-bit)
    localCanMsg.rtr = 0;   // Data frame (not remote transmission request)

    // NodeStatus structure (8 bytes total to fit in CAN 2.0 frame):
    // Bytes 0-3: uptime (uint32, little-endian)
    // Byte 4: health (uint2) + mode (uint3) + sub_mode (uint3) - packed in 1 byte
    // Byte 5: vendor_specific_status_code (uint16 LSB)
    // Byte 6: vendor_specific_status_code (uint16 MSB) - can be 0
    // Byte 7: tail byte (SOF=1, EOF=1, Toggle=0, Transfer ID)

    localCanMsg.data[0] = (uint8_t)(uptimeSec & 0xFF);
    localCanMsg.data[1] = (uint8_t)((uptimeSec >> 8) & 0xFF);
    localCanMsg.data[2] = (uint8_t)((uptimeSec >> 16) & 0xFF);
    localCanMsg.data[3] = (uint8_t)((uptimeSec >> 24) & 0xFF);

    // Pack health (2 bits), mode (3 bits), sub_mode (3 bits) into byte 4
    // health: 0=OK, mode: 0=OPERATIONAL, sub_mode: 0=normal
    localCanMsg.data[4] = 0x00; // health=0, mode=0, sub_mode=0

    // vendor_specific_status_code (uint16, little-endian) - can be 0
    localCanMsg.data[5] = 0x00; // LSB
    localCanMsg.data[6] = 0x00; // MSB

    // Tail byte (SOF=1, EOF=1, Toggle=0, Transfer ID)
    localCanMsg.data[7] = 0xC0 | (transferId & 0x1F);

    localCanMsg.data_length_code = 8;  // CAN 2.0 maximum

    Serial.print("[Tmotor] TX: NodeStatus announcement - CAN ID=0x");
    Serial.print(canId, HEX);
    Serial.print(" Uptime=");
    Serial.print(uptimeSec);
    Serial.print("s DLC=");
    Serial.print(localCanMsg.data_length_code);
    Serial.print(" Data=[");
    for (uint8_t i = 0; i < localCanMsg.data_length_code; i++) {
        if (i > 0) Serial.print(" ");
        if (localCanMsg.data[i] < 0x10) Serial.print("0");
        Serial.print(localCanMsg.data[i], HEX);
    }
    Serial.print("]");

    // Check driver status before transmitting
    twai_status_info_t status_info;
    twai_get_status_info(&status_info);
    Serial.print(" TXErr=");
    Serial.print(status_info.tx_error_counter);
    Serial.print(" RXErr=");
    Serial.print(status_info.rx_error_counter);

    esp_err_t tx_result = twai_transmit(&localCanMsg, pdMS_TO_TICKS(100));  // Increased timeout to 100ms
    if (tx_result == ESP_OK) {
        Serial.println(" - OK");
    } else {
        Serial.print(" - ERROR: ");
        if (tx_result == ESP_ERR_INVALID_STATE) {
            Serial.println("INVALID_STATE (driver not ready)");
        } else if (tx_result == ESP_ERR_INVALID_ARG) {
            Serial.println("INVALID_ARG (bad message format)");
        } else if (tx_result == ESP_ERR_TIMEOUT) {
            Serial.println("TIMEOUT (TX queue full)");
        } else {
            Serial.print("Code ");
            Serial.println(tx_result);
        }
    }
    transferId++;
}

void Tmotor::requestNodeInfo() {
    Serial.println("[Tmotor] requestNodeInfo() called");

    // Send GetNodeInfo service request to discover ESC capabilities
    // Service ID for GetNodeInfo is 1 (uavcan.protocol.GetNodeInfo)
    twai_message_t localCanMsg;
    memset(&localCanMsg, 0, sizeof(twai_message_t));  // Initialize all fields to zero

    // DroneCAN service frame format for GetNodeInfo request:
    // [28:26] Priority (3 bits) = 0
    // [25:16] Service Type ID (10 bits) = 1 (GetNodeInfo)
    // [15]    Request/Response (1 bit) = 1 (Request)
    // [14:8]  Destination Node ID (7 bits) = 1 (ESC node)
    // [7]     Service/Message (1 bit) = 1 (Service)
    // [6:0]   Source Node ID (7 bits) = 0x13 (our node)

    uint32_t canId = 0;
    canId |= ((uint32_t)0 << 26);              // Priority = 0
    canId |= ((uint32_t)1 << 16);              // Service Type ID = 1 (GetNodeInfo)
    canId |= (1U << 15);                       // Request (not response)
    canId |= ((uint32_t)1 << 8);               // Destination Node ID = 1 (ESC)
    canId |= (1U << 7);                        // Service frame
    canId |= (uint32_t)(nodeId & 0x7F);        // Source Node ID

    localCanMsg.identifier = canId;
    localCanMsg.extd = 1;  // Extended frame (29-bit)
    localCanMsg.rtr = 0;   // Data frame (not remote transmission request)

    // GetNodeInfo request has no payload (empty request)
    localCanMsg.data[0] = 0xC0 | (transferId & 0x1F);  // Tail byte (SOF=1, EOF=1, TID)
    localCanMsg.data_length_code = 1;

    Serial.print("[Tmotor] TX: GetNodeInfo request to ESC (Node 1) - CAN ID=0x");
    Serial.print(canId, HEX);
    Serial.print(" DLC=");
    Serial.print(localCanMsg.data_length_code);
    Serial.print(" Data=[");
    for (uint8_t i = 0; i < localCanMsg.data_length_code; i++) {
        if (i > 0) Serial.print(" ");
        if (localCanMsg.data[i] < 0x10) Serial.print("0");
        Serial.print(localCanMsg.data[i], HEX);
    }
    Serial.print("]");

    // Check driver status
    twai_status_info_t status_info;
    twai_get_status_info(&status_info);
    Serial.print(" TXErr=");
    Serial.print(status_info.tx_error_counter);

    esp_err_t tx_result = twai_transmit(&localCanMsg, pdMS_TO_TICKS(100));  // Increased timeout
    if (tx_result == ESP_OK) {
        Serial.println(" - OK");
    } else {
        Serial.print(" - ERROR: ");
        if (tx_result == ESP_ERR_INVALID_STATE) {
            Serial.println("INVALID_STATE");
        } else if (tx_result == ESP_ERR_INVALID_ARG) {
            Serial.println("INVALID_ARG");
        } else if (tx_result == ESP_ERR_TIMEOUT) {
            Serial.println("TIMEOUT");
        } else {
            Serial.print("Code ");
            Serial.println(tx_result);
        }
    }
    transferId++;
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
    Serial.println("========================================");
    Serial.println("[Tmotor] DEBUG: Enviando ParamCfg COM CRC");
    Serial.println("========================================");

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

    Serial.print("[DEBUG] CRC16 calculated: 0x");
    Serial.println(crc, HEX);

    // Build CAN ID for ParamCfg (1033)
    uint32_t dataTypeId = paramCfgDataTypeId;
    uint32_t priority = 0x18;  // LOW priority
    uint32_t service = 0;  // Message frame
    uint32_t srcNodeId = nodeId;  // 0x13

    uint32_t canId = (priority << 24) |
                     (dataTypeId << 8) |
                     (service << 7) |
                     (srcNodeId & 0x7F);

    Serial.print("[DEBUG] CAN ID: 0x");
    Serial.println(canId, HEX);

    twai_message_t localCanMsg;
    localCanMsg.identifier = canId;
    localCanMsg.extd = 1;
    localCanMsg.rtr = 0;
    localCanMsg.ss = 0;
    localCanMsg.self = 0;
    localCanMsg.dlc_non_comp = 0;

    // Save current TransferID (MUST be same for all frames!)
    uint8_t currentTransferId = transferId;
    Serial.print("[DEBUG] TransferID: ");
    Serial.println(currentTransferId);

    esp_err_t tx_result;

    // ===== MULTI-FRAME WITH CRC =====
    // Total: 27 bytes payload + 2 bytes CRC = 29 bytes
    // Frame 1: CRC (2 bytes) + payload[0-4] (5 bytes) = 7 bytes
    // Frame 2: payload[5-11] (7 bytes)
    // Frame 3: payload[12-18] (7 bytes)
    // Frame 4: payload[19-25] (7 bytes)
    // Frame 5: payload[26] (1 byte)

    // Frame 1: CRC + payload[0-4]
    Serial.println("[DEBUG] Frame 1: CRC + payload[0-4]");
    localCanMsg.data[0] = (uint8_t)(crc & 0xFF);         // CRC low byte
    localCanMsg.data[1] = (uint8_t)((crc >> 8) & 0xFF);  // CRC high byte
    localCanMsg.data[2] = payload[0];  // esc_index
    localCanMsg.data[3] = payload[1];  // esc_uuid[0]
    localCanMsg.data[4] = payload[2];  // esc_uuid[1]
    localCanMsg.data[5] = payload[3];  // esc_uuid[2]
    localCanMsg.data[6] = payload[4];  // esc_uuid[3]
    localCanMsg.data[7] = 0x80 | (currentTransferId & 0x1F);  // SOF=1, EOF=0, Toggle=0
    localCanMsg.data_length_code = 8;

    Serial.print("  Data: [");
    for (int i = 0; i < 8; i++) {
        if (localCanMsg.data[i] < 0x10) Serial.print("0");
        Serial.print(localCanMsg.data[i], HEX);
        if (i < 7) Serial.print(" ");
    }
    Serial.println("]");

    tx_result = twai_transmit(&localCanMsg, pdMS_TO_TICKS(100));
    if (tx_result != ESP_OK) {
        Serial.println("[ERROR] Frame 1 TX failed");
        return;
    }
    Serial.println("  OK");
    delay(2);

    // Frame 2: payload[5-11]
    Serial.println("[DEBUG] Frame 2: payload[5-11]");
    localCanMsg.data[0] = payload[5];
    localCanMsg.data[1] = payload[6];
    localCanMsg.data[2] = payload[7];
    localCanMsg.data[3] = payload[8];
    localCanMsg.data[4] = payload[9];
    localCanMsg.data[5] = payload[10];
    localCanMsg.data[6] = payload[11];
    localCanMsg.data[7] = 0x20 | (currentTransferId & 0x1F);  // SOF=0, EOF=0, Toggle=1
    localCanMsg.data_length_code = 8;

    Serial.print("  Data: [");
    for (int i = 0; i < 8; i++) {
        if (localCanMsg.data[i] < 0x10) Serial.print("0");
        Serial.print(localCanMsg.data[i], HEX);
        if (i < 7) Serial.print(" ");
    }
    Serial.println("]");

    tx_result = twai_transmit(&localCanMsg, pdMS_TO_TICKS(100));
    if (tx_result != ESP_OK) {
        Serial.println("[ERROR] Frame 2 TX failed");
        return;
    }
    Serial.println("  OK");
    delay(2);

    // Frame 3: payload[12-18]
    Serial.println("[DEBUG] Frame 3: payload[12-18]");
    localCanMsg.data[0] = payload[12];
    localCanMsg.data[1] = payload[13];
    localCanMsg.data[2] = payload[14];
    localCanMsg.data[3] = payload[15];
    localCanMsg.data[4] = payload[16];
    localCanMsg.data[5] = payload[17];
    localCanMsg.data[6] = payload[18];
    localCanMsg.data[7] = 0x00 | (currentTransferId & 0x1F);  // SOF=0, EOF=0, Toggle=0
    localCanMsg.data_length_code = 8;

    Serial.print("  Data: [");
    for (int i = 0; i < 8; i++) {
        if (localCanMsg.data[i] < 0x10) Serial.print("0");
        Serial.print(localCanMsg.data[i], HEX);
        if (i < 7) Serial.print(" ");
    }
    Serial.println("]");

    tx_result = twai_transmit(&localCanMsg, pdMS_TO_TICKS(100));
    if (tx_result != ESP_OK) {
        Serial.println("[ERROR] Frame 3 TX failed");
        return;
    }
    Serial.println("  OK");
    delay(2);

    // Frame 4: payload[19-25]
    Serial.println("[DEBUG] Frame 4: payload[19-25]");
    localCanMsg.data[0] = payload[19];
    localCanMsg.data[1] = payload[20];
    localCanMsg.data[2] = payload[21];
    localCanMsg.data[3] = payload[22];
    localCanMsg.data[4] = payload[23];
    localCanMsg.data[5] = payload[24];  // esc_fdb_rate low
    localCanMsg.data[6] = payload[25];  // esc_fdb_rate high
    localCanMsg.data[7] = 0x20 | (currentTransferId & 0x1F);  // SOF=0, EOF=0, Toggle=1
    localCanMsg.data_length_code = 8;

    Serial.print("  Data: [");
    for (int i = 0; i < 8; i++) {
        if (localCanMsg.data[i] < 0x10) Serial.print("0");
        Serial.print(localCanMsg.data[i], HEX);
        if (i < 7) Serial.print(" ");
    }
    Serial.println("]");

    tx_result = twai_transmit(&localCanMsg, pdMS_TO_TICKS(100));
    if (tx_result != ESP_OK) {
        Serial.println("[ERROR] Frame 4 TX failed");
        return;
    }
    Serial.println("  OK");
    delay(2);

    // Frame 5: payload[26] (LAST FRAME)
    Serial.println("[DEBUG] Frame 5: payload[26] (LAST)");
    localCanMsg.data[0] = payload[26];  // esc_save_option
    localCanMsg.data[1] = 0x00;  // Padding
    localCanMsg.data[2] = 0x00;
    localCanMsg.data[3] = 0x00;
    localCanMsg.data[4] = 0x00;
    localCanMsg.data[5] = 0x00;
    localCanMsg.data[6] = 0x00;
    localCanMsg.data[7] = 0x40 | (currentTransferId & 0x1F);  // SOF=0, EOF=1, Toggle=0
    localCanMsg.data_length_code = 8;

    Serial.print("  Data: [");
    for (int i = 0; i < 8; i++) {
        if (localCanMsg.data[i] < 0x10) Serial.print("0");
        Serial.print(localCanMsg.data[i], HEX);
        if (i < 7) Serial.print(" ");
    }
    Serial.println("]");

    tx_result = twai_transmit(&localCanMsg, pdMS_TO_TICKS(100));
    if (tx_result != ESP_OK) {
        Serial.println("[ERROR] Frame 5 TX failed");
        return;
    }
    Serial.println("  OK");

    // Increment TransferID only AFTER all frames sent
    transferId = (transferId + 1) & 0x1F;

    Serial.println("========================================");
    Serial.println("[Tmotor] ParamCfg COM CRC enviado!");
    Serial.print("  CRC16: 0x");
    Serial.println(crc, HEX);
    Serial.print("  TransferID: ");
    Serial.println(currentTransferId);
    Serial.print("  esc_fdb_rate: ");
    Serial.print(feedbackRate);
    Serial.println(" Hz");
    Serial.println("  Total: 5 frames (com CRC)");
    Serial.println("========================================");
    Serial.println();
    Serial.println("[IMPORTANTE] Aguardando ParamGet (1332)...");
    Serial.println();
}

void Tmotor::handleNodeStatus(twai_message_t *canMsg) {
    // Extract Node ID from CAN ID
    uint8_t sourceNodeId = CanUtils::getNodeIdFromCanId(canMsg->identifier);

    // Ignore our own NodeStatus (nodeId = 0x13)
    if (sourceNodeId == nodeId) {
        return;
    }

    Serial.print("[Tmotor] handleNodeStatus: Received from Node ID ");
    Serial.print(sourceNodeId, HEX);
    Serial.println(" (ESC detected)");

    // Store detected ESC Node ID
    if (detectedEscNodeId == 0) {
        detectedEscNodeId = sourceNodeId;
        Serial.print("[Tmotor] ESC Node ID detected: ");
        Serial.println(detectedEscNodeId, HEX);
    }

    // Check if we already sent ParamCfg recently (within last 5 seconds)
    // This prevents spamming the bus with configuration messages
    if (paramCfgSent && (millis() - lastParamCfgSent < 5000)) {
        return;  // Already configured recently, skip
    }

    // Send ParamCfg to configure telemetry rate
    // Use 50 Hz as default feedback rate (conservative, avoids bus overload)
    Serial.println("[Tmotor] Sending ParamCfg to configure ESC telemetry rate...");
    sendParamCfg(detectedEscNodeId, 50, true);  // 50 Hz, permanent save

    paramCfgSent = true;
    lastParamCfgSent = millis();
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
    uint32_t srcNodeId = nodeId;

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
    esp_err_t tx_result = twai_transmit(&localCanMsg, pdMS_TO_TICKS(100));

    if (tx_result == ESP_OK) {
        Serial.print("[Tmotor] TX: RawCommand (1030) - Throttle=");
        Serial.print(throttle);
        Serial.print(" CAN ID=0x");
        Serial.print(canId, HEX);
        Serial.println(" - OK");
    } else {
        Serial.print("[Tmotor] TX: RawCommand ERROR: ");
        Serial.println(tx_result);
    }

    transferId = (transferId + 1) & 0x1F;
}
