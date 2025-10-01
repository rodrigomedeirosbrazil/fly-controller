#include <Arduino.h>
#include <driver/twai.h>

#include "../config.h"
#include "Hobbywing.h"

extern twai_message_t canMsg;

Hobbywing::Hobbywing() {
    lastReadStatusMsg1 = 0;
    lastReadStatusMsg2 = 0;
    lastAnnounce = millis();
    transferId = 0;

    temperature = 0;
    deciCurrent = 0;
    deciVoltage = 0;
    rpm = 0;
    isCcwDirection = false;
}

void Hobbywing::parseEscMessage(twai_message_t *canMsg) {
    // Extract DataType ID from CAN ID (assuming DroneCAN format)
    uint16_t dataTypeId = getDataTypeIdFromCanId(canMsg->identifier);

    // Handle ESC ID request/response messages
    if (dataTypeId == getEscIdRequestDataTypeId) {
        handleGetEscIdResponse(canMsg);
        return;
    }

    // Check if this is a Hobbywing ESC status message
    if (dataTypeId != statusMsg1 && dataTypeId != statusMsg2 && dataTypeId != statusMsg3) {
        return; // Not a Hobbywing ESC message
    }

    uint8_t tailByte = getTailByteFromPayload(canMsg->data, canMsg->data_length_code);

    // Validate frame structure
    if (!isStartOfFrame(tailByte) && !isEndOfFrame(tailByte) && isToggleFrame(tailByte)) {
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

bool Hobbywing::isReady() {
    return lastReadStatusMsg1 != 0
        && lastReadStatusMsg2 != 0
        && millis() - lastReadStatusMsg1 < 1000
        && millis() - lastReadStatusMsg2 < 1000;
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
    deciCurrent = getDeciCurrentFromPayload(canMsg->data);
    deciVoltage = getDeciVoltageFromPayload(canMsg->data);
    lastReadStatusMsg1 = millis();
}

uint8_t Hobbywing::getTailByteFromPayload(uint8_t *payload, uint8_t canDlc) {
    return payload[canDlc - 1];
}

bool Hobbywing::isStartOfFrame(uint8_t tailByte) {
    return (tailByte & 0x80) >> 7 == 1;
}

bool Hobbywing::isEndOfFrame(uint8_t tailByte) {
    return (tailByte & 0x40) >> 6 == 1;
}

bool Hobbywing::isToggleFrame(uint8_t tailByte) {
    return (tailByte & 0x20) >> 5 == 1;
}

uint8_t Hobbywing::getTemperatureFromPayload(uint8_t *payload) {
    return payload[4];
}

uint16_t Hobbywing::getDeciCurrentFromPayload(uint8_t *payload) {
    return (payload[3] << 8) | payload[2];
}

uint16_t Hobbywing::getDeciVoltageFromPayload(uint8_t *payload) {
    return (payload[1] << 8) | payload[0];
}

uint16_t Hobbywing::getRpmFromPayload(uint8_t *payload) {
    return (payload[1] << 8) | payload[0];
}

bool Hobbywing::getDirectionCCWFromPayload(uint8_t *payload) {
    return (payload[5] & 0x80) >> 7 == 1;
}

// ESC Control Methods

void Hobbywing::announce()
{
    if (millis() - lastAnnounce < 1000) {
        return;
    }

    lastAnnounce = millis();

    uint32_t uptimeSec = millis() / 1000;

    twai_message_t localCanMsg;
    uint32_t dataTypeId = 341;

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

void Hobbywing::setLedColor(uint8_t color, uint8_t blink)
{
    uint8_t data[3];
    data[0] = 0x00; // setLedOptionSave 0x01
    data[1] = color; // ledColorRed = 0x04 / ledColorGreen = 0x02 / ledColorBlue = 0x01
    data[2] = blink; // blinkOff = 0x00 / Blink1Hz = 0x01 / Blink2Hz = 0x02 / Blink5Hz = 0x05

    sendMessage(
        0x00, // priority
        0xd4, // serviceTypeId
        escNodeId,
        data,
        3
    );
}

void Hobbywing::setDirection(bool isCcw)
{
    uint8_t data[1];

    data[0] = isCcw ? 0x00 : 0x01;

    sendMessage(
        0x00, // priority
        0xD5, // serviceTypeId
        escNodeId,
        data,
        1
    );
}

void Hobbywing::setThrottleSource(uint8_t source)
{
    uint8_t data[1];

    data[0] = source;

    sendMessage(
        0x00, // priority
        0xD7, // serviceTypeId (0xD7 is 215 in hex)
        escNodeId,
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
    canId |= (nodeId & 0xFF);                  // Source Node ID
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
    canId |= (nodeId & 0xFF);                  // Source Node ID
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
    uint8_t sourceNodeId = getNodeIdFromCanId(canMsg->identifier);

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
    canId |= (uint32_t)(nodeId & 0x7F);

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

uint8_t Hobbywing::getTransferId(uint8_t tailByte) {
    return tailByte & 0x1F;
}

uint8_t Hobbywing::getEscThrottleIdFromPayload(uint8_t *payload) {
    return payload[1];
}

// CAN ID parsing methods

uint8_t Hobbywing::getPriorityFromCanId(uint32_t canId) {
    return (canId >> 24) & 0xFF;
}

uint16_t Hobbywing::getDataTypeIdFromCanId(uint32_t canId) {
    return (canId >> 8) & 0xFFFF;
}

uint8_t Hobbywing::getServiceTypeIdFromCanId(uint32_t canId) {
    return (canId >> 16) & 0xFF;
}

uint8_t Hobbywing::getNodeIdFromCanId(uint32_t canId) {
    return canId & 0x7F;
}

uint8_t Hobbywing::getDestNodeIdFromCanId(uint32_t canId) {
   return (canId >> 8) & 0x7F;
}

bool Hobbywing::isServiceFrame(uint32_t canId) {
    return (canId & 0x80) >> 7 == 1;
}

bool Hobbywing::isRequestFrame(uint32_t canId) {
    return (canId & 0x8000) != 0;
}
