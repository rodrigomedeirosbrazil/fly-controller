#include <Arduino.h>
#include <SPI.h>
#include <mcp2515.h>

#include "../config.h"
#include "Canbus.h"

Canbus::Canbus() {
    lastReadStatusMsg1 = 0;
    lastReadStatusMsg2 = 0;
    transferId = 0;

    temperature = 0;
    deciCurrent = 0;
    deciVoltage = 0;
    rpm = 0;
}

void Canbus::parseCanMsg(struct can_frame *canMsg) {

    uint16_t dataTypeId = getDataTypeIdFromCanId(canMsg->can_id);

    if (
        dataTypeId != statusMsg1
        && dataTypeId != statusMsg2
    ) {
        if (dataTypeId != statusMsg3) {
            Serial.print("Data Type ID: ");
            Serial.print(dataTypeId, HEX);
            Serial.print(" Node ID: ");
            Serial.println(getNodeIdFromCanId(canMsg->can_id), HEX);
        }

        return;
    }

    uint8_t tailByte = getTailByteFromPayload(canMsg->data, canMsg->can_dlc);

    if (
        ! isStartOfFrame(tailByte) &&
        ! isEndOfFrame(tailByte) &&
        isToggleFrame(tailByte)
    ) {
        return;
    }

    if (dataTypeId == statusMsg1) {
        handleStatusMsg1(canMsg);
        return;
    }

    if (dataTypeId == statusMsg2) {
        handleStatusMsg2(canMsg);
        return;
    }
}

bool Canbus::isReady() {
    return
        lastReadStatusMsg1 != 0
        && lastReadStatusMsg2 != 0
        && millis() - lastReadStatusMsg1 < 1000
        && millis() - lastReadStatusMsg2 < 1000;
}

void Canbus::handleStatusMsg1(struct can_frame *canMsg) {
    if (canMsg->can_dlc != 7) {
        return;
    }

    rpm = getRpmFromPayload(canMsg->data);
    isCcwDirection  = getDirectionCCWFromPayload(canMsg->data);
    lastReadStatusMsg2 = millis();
}

void Canbus::handleStatusMsg2(struct can_frame *canMsg) {
    if (canMsg->can_dlc != 6) {
        return;
    }

    temperature = getTemperatureFromPayload(canMsg->data);
    deciCurrent = getDeciCurrentFromPayload(canMsg->data);
    deciVoltage = getDeciVoltageFromPayload(canMsg->data);
    lastReadStatusMsg1 = millis();
}

uint8_t Canbus::getPriorityFromCanId(uint32_t canId) {
    return (canId >> 24) & 0xFF;
}

uint16_t Canbus::getDataTypeIdFromCanId(uint32_t canId) {
    return (canId >> 8) & 0xFFFF;
}

uint8_t Canbus::getServiceTypeIdFromCanId(uint32_t canId) {
    return (canId >> 16) & 0xFF;
}

uint8_t Canbus::getNodeIdFromCanId(uint32_t canId) {
    return canId & 0x7F;
}

uint8_t Canbus::getDestNodeIdFromCanId(uint32_t canId) {
   return (canId >> 8) & 0x7F;
}

bool Canbus::isServiceFrame(uint32_t canId) {
    return (canId & 0x80) >> 7 == 1;
}

bool Canbus::isRequestFrame(uint32_t canId) {
    return (canId & 0x8000) != 0;
}

uint8_t Canbus::getTailByteFromPayload(uint8_t *payload, uint8_t canDlc) {
    return payload[canDlc - 1];
}

bool Canbus::isStartOfFrame(uint8_t tailByte) {
    return (tailByte & 0x80) >> 7 == 1;
}

bool Canbus::isEndOfFrame(uint8_t tailByte) {
    return (tailByte & 0x40) >> 6 == 1;
}

bool Canbus::isToggleFrame(uint8_t tailByte) {
    return (tailByte & 0x20) >> 5 == 1;
}

uint8_t Canbus::getTransferId(uint8_t tailByte) {
    return tailByte & 0x1F;
}

uint8_t Canbus::getTemperatureFromPayload(uint8_t *payload) {
    return payload[4];
}

uint16_t Canbus::getDeciCurrentFromPayload(uint8_t *payload) {
    return (payload[3] << 8) | payload[2];
}

uint16_t Canbus::getDeciVoltageFromPayload(uint8_t *payload) {
    return (payload[1] << 8) | payload[0];
}

uint16_t Canbus::getRpmFromPayload(uint8_t *payload) {
    return (payload[1] << 8) | payload[0];
}

bool Canbus::getDirectionCCWFromPayload(uint8_t *payload) {
    return (payload[5] & 0x80) >> 7 == 1;
}

void Canbus::setLedColor(uint8_t color)
{
    uint8_t data[3];
    data[0] = 0x00; // setLedOptionSave 0x01
    data[1] = color; // ledColorRed = 0x04 / ledColorGreen = 0x02 / ledColorBlue = 0x01
    data[2] = 0x00; // blinkOff = 0x00 / Blink1Hz = 0x01 / Blink2Hz = 0x02 / Blink5Hz = 0x05

    sendMessage(
        0x00, // priority
        0xd4, // serviceTypeId
        escNodeId,
        data,
        3
    );
}

void Canbus::setDirection(bool isCcw)
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

void Canbus::setThrottleSource(bool isPwm)
{
    uint8_t data[1];

    // 0x00 = PWM, 0x01 = CAN
    data[0] = isPwm ? 0x00 : 0x01;

    sendMessage(
        0x00, // priority
        0xD7, // serviceTypeId (0xD7 is 215 in hex)
        escNodeId,
        data,
        1 // data length
    );
}

void Canbus::setThrottle(uint16_t value)
{
    // Hobbywing/DroneCAN ESC throttle command:
    // serviceTypeId: 0xD6 (214)
    // Payload: 2 bytes, little-endian, throttle value (0-2000)
    uint8_t data[2];
    data[0] = value & 0xFF;
    data[1] = (value >> 8) & 0xFF;

    sendMessage(
        0x00,      // priority
        0xD6,      // serviceTypeId for throttle
        escNodeId, // destination node ID (ESC)
        data,
        2          // payload length
    );
}

void Canbus::sendMessage(
    uint8_t priority,
    uint8_t serviceTypeId,
    uint8_t destNodeId,
    uint8_t *payload,
    uint8_t payloadLength
) {
    struct can_frame canMsg;

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

    // The MCP2515 library requires the CAN_EFF_FLAG to be set for extended frames.
    // The 29-bit DroneCAN ID is placed in the lower bits of the can_id field.
    canMsg.can_id = canId | CAN_EFF_FLAG;

    canMsg.can_dlc = payloadLength + 1;
    for (uint8_t i = 0; i < payloadLength; i++) {
        canMsg.data[i] = payload[i];
    }

    // Tail byte for a single-frame transfer:
    // SOF=1, EOF=1, Toggle=0, Transfer ID (5 bits)
    canMsg.data[payloadLength] = 0xC0 | (transferId & 0x1F);

    mcp2515.sendMessage(&canMsg);

    transferId++;
}
