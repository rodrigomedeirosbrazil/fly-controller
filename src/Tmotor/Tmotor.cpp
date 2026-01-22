#include <Arduino.h>
#include <driver/twai.h>

#include "../config.h"
#include "Tmotor.h"

extern twai_message_t canMsg;

Tmotor::Tmotor() {
    lastReadEscStatus = 0;
    lastReadPushCan = 0;
    lastAnnounce = millis();
    transferId = 0;

    escTemperature = 0;
    motorTemperature = 0;
    deciCurrent = 0;
    deciVoltage = 0;
    rpm = 0;
    errorCount = 0;
    powerRatingPct = 0;
    escIndex = 0;
}

void Tmotor::parseEscMessage(twai_message_t *canMsg) {
    // Extract DataType ID from CAN ID (assuming DroneCAN format)
    uint16_t dataTypeId = getDataTypeIdFromCanId(canMsg->identifier);

    // Route to appropriate handler
    if (dataTypeId == escStatusDataTypeId) {
        handleEscStatus(canMsg);
        return;
    }

    if (dataTypeId == pushCanDataTypeId) {
        handlePushCan(canMsg);
        return;
    }
}

bool Tmotor::isReady() {
    // Consider ready if we've received ESC_STATUS within the last second
    return lastReadEscStatus != 0
        && millis() - lastReadEscStatus < 1000;
}

void Tmotor::handleEscStatus(twai_message_t *canMsg) {
    // ESC_STATUS structure:
    // uint32 error_count (bytes 0-3)
    // float16 voltage (bytes 4-5)
    // float16 current (bytes 6-7)
    // float16 temperature (bytes 8-9) - ESC temperature in Kelvin
    // int18 rpm (bytes 10-12, 3 bytes)
    // uint7 power_rating_pct (bits 0-6 of byte 13)
    // uint5 esc_index (bits 0-4 of byte 14, or bits 3-7 of byte 13 depending on packing)
    // tail byte (last byte)

    uint8_t tailByte = getTailByteFromPayload(canMsg->data, canMsg->data_length_code);

    // Validate frame structure - must be start or end frame
    if (!isStartOfFrame(tailByte) && !isEndOfFrame(tailByte)) {
        return;
    }

    // Minimum payload size: 4 (error_count) + 2 (voltage) + 2 (current) + 2 (temp) + 3 (rpm) + 1 (power/index) + 1 (tail) = 15 bytes
    if (canMsg->data_length_code < 15) {
        return;
    }

    // Extract error_count (uint32, little-endian)
    errorCount = (uint32_t)canMsg->data[0] |
                 ((uint32_t)canMsg->data[1] << 8) |
                 ((uint32_t)canMsg->data[2] << 16) |
                 ((uint32_t)canMsg->data[3] << 24);

    // Extract voltage (float16, bytes 4-5)
    uint16_t voltageFloat16 = (uint16_t)canMsg->data[4] | ((uint16_t)canMsg->data[5] << 8);
    float voltage = convertFloat16ToFloat(voltageFloat16);
    deciVoltage = (uint16_t)(voltage * 10.0f + 0.5f);  // Convert to decivolts

    // Extract current (float16, bytes 6-7)
    uint16_t currentFloat16 = (uint16_t)canMsg->data[6] | ((uint16_t)canMsg->data[7] << 8);
    float current = convertFloat16ToFloat(currentFloat16);
    deciCurrent = (uint16_t)(current * 10.0f + 0.5f);  // Convert to decicurrent

    // Extract ESC temperature (float16, bytes 8-9) - in Kelvin
    uint16_t tempFloat16 = (uint16_t)canMsg->data[8] | ((uint16_t)canMsg->data[9] << 8);
    float tempKelvin = convertFloat16ToFloat(tempFloat16);
    escTemperature = (uint8_t)(tempKelvin - 273.15f + 0.5f);  // Convert Kelvin to Celsius

    // Extract RPM (int18, bytes 10-12, signed 3-byte value)
    // int18 is a signed 18-bit integer, stored in 3 bytes (little-endian)
    int32_t rpmRaw = (int32_t)canMsg->data[10] |
                     ((int32_t)canMsg->data[11] << 8) |
                     ((int32_t)canMsg->data[12] << 16);
    // Sign extend from 18 bits to 32 bits
    if (rpmRaw & 0x20000) {  // Check sign bit (bit 17)
        rpmRaw |= 0xFFFC0000;  // Sign extend
    }
    rpm = (uint16_t)abs(rpmRaw);

    // Extract power_rating_pct and esc_index (byte 13)
    // According to DSDL: uint7 power_rating_pct, uint5 esc_index
    // Assuming they're packed in byte 13: power_rating_pct in bits 0-6, esc_index in bits 7-11 (but that's 5 bits, so might be in next byte)
    // Actually, looking at typical packing: power_rating_pct (7 bits) + esc_index (5 bits) = 12 bits, so might span bytes
    // For now, assume power_rating_pct is in byte 13 bits 0-6, and esc_index is in byte 14 bits 0-4
    if (canMsg->data_length_code >= 15) {
        powerRatingPct = canMsg->data[13] & 0x7F;  // Lower 7 bits
        if (canMsg->data_length_code >= 16) {
            escIndex = canMsg->data[14] & 0x1F;  // Lower 5 bits (before tail byte)
        }
    }

    lastReadEscStatus = millis();
}

void Tmotor::handlePushCan(twai_message_t *canMsg) {
    // PUSHCAN structure:
    // uint32 data_sequence (bytes 0-3)
    // struct { uint8_t len; uint8_t data[255]; }data;
    // The motor temperature is in bytes 19-20 of the data array

    uint8_t tailByte = getTailByteFromPayload(canMsg->data, canMsg->data_length_code);

    // Validate frame structure
    if (!isStartOfFrame(tailByte) && !isEndOfFrame(tailByte)) {
        return;
    }

    // Minimum size: 4 (data_sequence) + 1 (len) + 20 (to reach bytes 19-20) + 1 (tail) = 26 bytes
    if (canMsg->data_length_code < 26) {
        return;
    }

    // Extract data length (byte 4, after data_sequence)
    uint8_t dataLen = canMsg->data[4];

    // Check if we have enough data to read bytes 19-20
    if (dataLen < 21) {
        return;
    }

    // Extract motor temperature from bytes 19-20 of data array
    // data array starts at byte 5 (after data_sequence and len)
    // So bytes 19-20 of data array are at indices 5+19=24 and 5+20=25
    // But we need to account for data_sequence (4 bytes) and len (1 byte) = 5 bytes offset
    // So data[19] is at index 5+19=24, data[20] is at index 5+20=25
    uint16_t motorTempValue = (uint16_t)canMsg->data[24] | ((uint16_t)canMsg->data[25] << 8);

    // Assuming temperature is in float16 format (similar to ESC_STATUS)
    // If it's in Kelvin, convert to Celsius
    float tempKelvin = convertFloat16ToFloat(motorTempValue);
    motorTemperature = (uint8_t)(tempKelvin - 273.15f + 0.5f);  // Convert Kelvin to Celsius

    lastReadPushCan = millis();
}

void Tmotor::announce() {
    if (millis() - lastAnnounce < 1000) {
        return;
    }

    lastAnnounce = millis();

    uint32_t uptimeSec = millis() / 1000;

    twai_message_t localCanMsg;
    uint32_t dataTypeId = nodeStatusDataTypeId;

    uint32_t canId = 0;
    canId |= ((uint32_t)0 << 26);              // Priority
    canId |= ((uint32_t)dataTypeId << 8);      // DataType ID
    canId |= (uint32_t)(nodeId & 0xFF);        // Source node ID
    localCanMsg.identifier = canId;
    localCanMsg.extd = 1;  // Extended frame (29-bit)

    localCanMsg.data[0] = (uint8_t)(uptimeSec & 0xFF);
    localCanMsg.data[1] = (uint8_t)((uptimeSec >> 8) & 0xFF);
    localCanMsg.data[2] = (uint8_t)((uptimeSec >> 16) & 0xFF);
    localCanMsg.data[3] = (uint8_t)((uptimeSec >> 24) & 0xFF);
    localCanMsg.data[4] = 0x00; // health: OK
    localCanMsg.data[5] = 0x00; // mode: operational
    localCanMsg.data[6] = 0x00; // sub_mode: normal
    localCanMsg.data[7] = 0x00; // vendor_specific_status_code LSB (rest can be zero)

    // Tail byte (SOF=1, EOF=1, Toggle=0, Transfer ID)
    localCanMsg.data[8] = 0xC0 | (transferId & 0x1F);

    localCanMsg.data_length_code = 9;

    twai_transmit(&localCanMsg, pdMS_TO_TICKS(10));
    transferId++;
}

void Tmotor::setRawThrottle(int16_t throttle) {
    twai_message_t localCanMsg;

    // DroneCAN DataType ID for uavcan.equipment.esc.RawCommand
    uint32_t dataTypeId = rawCommandDataTypeId;

    // CAN ID
    uint32_t canId = 0;
    canId |= ((uint32_t)0 << 26);              // Priority = 0
    canId |= (dataTypeId << 8);                // DataType ID
    canId |= (nodeId & 0xFF);                  // Source Node ID
    localCanMsg.identifier = canId;
    localCanMsg.extd = 1;  // Extended frame (29-bit)

    // RawCommand structure: struct { uint8_t len; int14_t data[20]; }cmd;
    // For single ESC, we send len=1 and data[0]=throttle value
    // int14_t is a signed 14-bit integer, stored in 2 bytes (little-endian)
    // Range: -8192 to 8191
    // Constrain throttle to int14_t range
    int16_t constrainedThrottle = constrain(throttle, -8192, 8191);

    localCanMsg.data[0] = 1;  // len = 1 (one ESC)
    localCanMsg.data[1] = constrainedThrottle & 0xFF;  // LSB
    localCanMsg.data[2] = (constrainedThrottle >> 8) & 0xFF;  // MSB (receiver will ignore upper 2 bits)

    // Tail byte
    localCanMsg.data[3] = 0xC0 | (transferId & 0x1F);
    localCanMsg.data_length_code = 4;

    twai_transmit(&localCanMsg, pdMS_TO_TICKS(10));
    transferId++;
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

// Helper methods

uint8_t Tmotor::getTailByteFromPayload(uint8_t *payload, uint8_t canDlc) {
    return payload[canDlc - 1];
}

bool Tmotor::isStartOfFrame(uint8_t tailByte) {
    return (tailByte & 0x80) >> 7 == 1;
}

bool Tmotor::isEndOfFrame(uint8_t tailByte) {
    return (tailByte & 0x40) >> 6 == 1;
}

bool Tmotor::isToggleFrame(uint8_t tailByte) {
    return (tailByte & 0x20) >> 5 == 1;
}

// CAN ID parsing methods

uint8_t Tmotor::getPriorityFromCanId(uint32_t canId) {
    return (canId >> 24) & 0xFF;
}

uint16_t Tmotor::getDataTypeIdFromCanId(uint32_t canId) {
    return (canId >> 8) & 0xFFFF;
}

uint8_t Tmotor::getNodeIdFromCanId(uint32_t canId) {
    return canId & 0x7F;
}
