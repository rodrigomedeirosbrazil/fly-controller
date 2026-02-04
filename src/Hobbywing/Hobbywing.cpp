#include <Arduino.h>
#include <driver/twai.h>

#include "../config.h"
#include "Hobbywing.h"
#include "../Canbus/CanUtils.h"
#include "../Throttle/Throttle.h"

extern twai_message_t canMsg;

Hobbywing::Hobbywing() {
    lastReadStatusMsg1 = 0;
    lastReadStatusMsg2 = 0;
    transferId = 0;

    temperature = 0;
    batteryCurrent = 0;
    batteryVoltageMilliVolts = 0;
    rpm = 0;
    isCcwDirection = false;
}

void Hobbywing::parseEscMessage(twai_message_t *canMsg) {
    // Extract DataType ID from CAN ID (assuming DroneCAN format)
    uint16_t dataTypeId = CanUtils::getDataTypeIdFromCanId(canMsg->identifier);

    // Handle ESC ID request/response messages
    if (dataTypeId == getEscIdRequestDataTypeId) {
        handleGetEscIdResponse(canMsg);
        return;
    }

    // Check if this is a Hobbywing ESC status message
    if (dataTypeId != statusMsg1 && dataTypeId != statusMsg2 && dataTypeId != statusMsg3) {
        return; // Not a Hobbywing ESC message
    }

    uint8_t tailByte = CanUtils::getTailByteFromPayload(canMsg->data, canMsg->data_length_code);

    // Validate frame structure
    if (!CanUtils::isStartOfFrame(tailByte) && !CanUtils::isEndOfFrame(tailByte) && CanUtils::isToggleFrame(tailByte)) {
        return;
    }

    // Route to appropriate handler
    if (dataTypeId == statusMsg1) {
        handleStatusMsg1(canMsg);
        return;
    }

    if (dataTypeId == statusMsg2) {
        handleStatusMsg2(canMsg);
        return;
    }
}

bool Hobbywing::hasTelemetry() {
    return lastReadStatusMsg1 != 0
        && lastReadStatusMsg2 != 0
        && millis() - lastReadStatusMsg1 < 1000
        && millis() - lastReadStatusMsg2 < 1000;
}

bool Hobbywing::isReady() {
    // ESC is ready if it has been detected via NodeStatus
    extern Canbus canbus;
    return canbus.getEscNodeId() != 0;
}

void Hobbywing::handleStatusMsg1(twai_message_t *canMsg) {
    if (canMsg->data_length_code != 7) {
        return;
    }

    rpm = getRpmFromPayload(canMsg->data);
    isCcwDirection = getDirectionCCWFromPayload(canMsg->data);

    lastReadStatusMsg2 = millis();
}

void Hobbywing::handleStatusMsg2(twai_message_t *canMsg) {
    if (canMsg->data_length_code != 6) {
        return;
    }

    temperature = getTemperatureFromPayload(canMsg->data);
    batteryCurrent = getBatteryCurrentFromPayload(canMsg->data);
    batteryVoltageMilliVolts = getBatteryVoltageMilliVoltsFromPayload(canMsg->data);
    lastReadStatusMsg1 = millis();
}


uint8_t Hobbywing::getTemperatureFromPayload(uint8_t *payload) {
    return payload[4];
}

uint8_t Hobbywing::getBatteryCurrentFromPayload(uint8_t *payload) {
    // Extract decicurrents from CAN payload
    uint16_t deciCurrent = (payload[3] << 8) | payload[2];
    // Convert to amperes: deciCurrent / 10 (round to nearest integer)
    // Maximum expected: 200A = 2000 decicurrents, so 2000 / 10 = 200A < 255 (uint8_t max)
    return (uint8_t)((deciCurrent + 5) / 10);  // Round to nearest integer
}

uint16_t Hobbywing::getBatteryVoltageMilliVoltsFromPayload(uint8_t *payload) {
    // Extract decivolts from CAN payload
    uint16_t deciVolts = (payload[1] << 8) | payload[0];
    // Convert to millivolts: deciVolts * 100
    // Maximum expected: 60V = 600 decivolts, so 600 * 100 = 60000 mV < 65535 (uint16_t max)
    return (uint16_t)(deciVolts * 100);
}

uint16_t Hobbywing::getRpmFromPayload(uint8_t *payload) {
    return (payload[1] << 8) | payload[0];
}

bool Hobbywing::getDirectionCCWFromPayload(uint8_t *payload) {
    return (payload[5] & 0x80) >> 7 == 1;
}

// ESC Control Methods

void Hobbywing::setLedColor(uint8_t color, uint8_t blink)
{
    uint8_t data[3];
    data[0] = 0x00; // setLedOptionSave 0x01
    data[1] = color; // ledColorRed = 0x04 / ledColorGreen = 0x02 / ledColorBlue = 0x01
    data[2] = blink; // blinkOff = 0x00 / Blink1Hz = 0x01 / Blink2Hz = 0x02 / Blink5Hz = 0x05

    // Use detected ESC node ID if available, otherwise fallback to hardcoded
    extern Canbus canbus;
    uint8_t targetNodeId = (canbus.getEscNodeId() != 0) ? canbus.getEscNodeId() : escNodeId;
    sendMessage(
        0x00, // priority
        0xd4, // serviceTypeId
        targetNodeId,
        data,
        3
    );
}

void Hobbywing::setDirection(bool isCcw)
{
    uint8_t data[1];

    data[0] = isCcw ? 0x00 : 0x01;

    // Use detected ESC node ID if available, otherwise fallback to hardcoded
    extern Canbus canbus;
    uint8_t targetNodeId = (canbus.getEscNodeId() != 0) ? canbus.getEscNodeId() : escNodeId;
    sendMessage(
        0x00, // priority
        0xD5, // serviceTypeId
        targetNodeId,
        data,
        1
    );
}

void Hobbywing::setThrottleSource(uint8_t source)
{
    uint8_t data[1];

    data[0] = source;

    // Use detected ESC node ID if available, otherwise fallback to hardcoded
    extern Canbus canbus;
    uint8_t targetNodeId = (canbus.getEscNodeId() != 0) ? canbus.getEscNodeId() : escNodeId;
    sendMessage(
        0x00, // priority
        0xD7, // serviceTypeId (0xD7 is 215 in hex)
        targetNodeId,
        data,
        1 // data length
    );
}

void Hobbywing::setRawThrottle(int16_t throttle)
{
    twai_message_t localCanMsg;

    // DroneCAN DataType ID for uavcan.equipment.esc.RawCommand
    uint32_t dataTypeId = 1030;

    // CAN ID
    uint32_t canId = 0;
    canId |= ((uint32_t)0 << 26);              // Priority = 0
    canId |= (dataTypeId << 8);                // DataType ID
    canId |= (canbus.getNodeId() & 0xFF);                  // Source Node ID
    localCanMsg.identifier = canId;
    localCanMsg.extd = 1;  // Extended frame (29-bit)

    // Create array with value only in escThrottleId position
    // All previous positions must exist (even with zero)
    uint8_t throttleArraySize = escThrottleId + 1;
    uint8_t byteIndex = 0;

    for (uint8_t i = 0; i < throttleArraySize; i++) {
        int16_t value = (i == escThrottleId) ? throttle : 0;
        localCanMsg.data[byteIndex++] = value & 0xFF;
        localCanMsg.data[byteIndex++] = (value >> 8) & 0xFF;
    }

    // Tail byte
    localCanMsg.data[byteIndex++] = 0xC0 | (transferId & 0x1F);
    localCanMsg.data_length_code = byteIndex;

    twai_transmit(&localCanMsg, pdMS_TO_TICKS(10));
    transferId++;
}

void Hobbywing::requestEscId() {
    twai_message_t localCanMsg;

    uint32_t dataTypeId = 20013;
    uint32_t canId = 0;
    canId |= ((uint32_t)0 << 26);              // Priority
    canId |= (dataTypeId << 8);                // DataType ID
    canId |= (canbus.getNodeId() & 0xFF);                  // Source Node ID
    localCanMsg.identifier = canId;
    localCanMsg.extd = 1;  // Extended frame (29-bit)

    // Payload can be empty or 1-3 bytes (according to ESC)
    uint8_t i = 0;
    localCanMsg.data[i++] = 0xC0 | (transferId & 0x1F); // Tail byte
    localCanMsg.data_length_code = i;

    Serial.println("Sending GetEscID as broadcast message");
    twai_transmit(&localCanMsg, pdMS_TO_TICKS(10));
    transferId++;
}

void Hobbywing::handleGetEscIdResponse(twai_message_t *canMsg) {
    if (canMsg->data_length_code < 7) {
        Serial.println("WARN: GetEscID response payload too short.");
        return;
    }

    uint8_t escThrottleIdReceived = getEscThrottleIdFromPayload(canMsg->data);
    uint8_t sourceNodeId = CanUtils::getNodeIdFromCanId(canMsg->identifier);

    Serial.print("Received GetEscID response from Node ID: ");
    Serial.print(sourceNodeId, HEX);
    Serial.print(" with Throttle ID: ");
    Serial.println(escThrottleIdReceived);
}

void Hobbywing::sendMessage(
    uint8_t priority,
    uint16_t serviceTypeId,
    uint8_t destNodeId,
    uint8_t *payload,
    uint8_t payloadLength
) {
    twai_message_t localCanMsg;

    // DroneCAN CAN ID structure for a service frame (request)
    // [28:26] Priority (3 bits)
    // [25:16] Service ID (10 bits)
    // [15]    Request/Response (1 bit, 1 for request)
    // [14:8]  Destination Node ID (7 bits)
    // [7]     Service/Message (1 bit, 1 for service)
    // [6:0]   Source Node ID (7 bits)

    uint32_t canId = 0;
    canId |= ((uint32_t)(priority & 0x07)) << 26;
    canId |= ((uint32_t)(serviceTypeId & 0x03FF)) << 16;
    canId |= (1U << 15); // Request, not response
    canId |= ((uint32_t)(destNodeId & 0x7F)) << 8;
    canId |= (1U << 7);  // Service, not message
    canId |= (uint32_t)(canbus.getNodeId() & 0x7F);

    // Note: CanUtils functions are available but we construct CAN ID directly here

    // TWAI uses extended frame flag (29-bit identifier)
    localCanMsg.identifier = canId;
    localCanMsg.extd = 1;  // Extended frame (29-bit)

    localCanMsg.data_length_code = payloadLength + 1;
    for (uint8_t i = 0; i < payloadLength; i++) {
        localCanMsg.data[i] = payload[i];
    }

    // Tail byte for a single-frame transfer:
    // SOF=1, EOF=1, Toggle=0, Transfer ID (5 bits)
    localCanMsg.data[payloadLength] = 0xC0 | (transferId & 0x1F);

    twai_transmit(&localCanMsg, pdMS_TO_TICKS(10));

    transferId++;
}

// Helper methods

uint8_t Hobbywing::getEscThrottleIdFromPayload(uint8_t *payload) {
    return payload[1];
}

void Hobbywing::handle() {
}
