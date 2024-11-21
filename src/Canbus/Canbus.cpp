#include <Arduino.h>
#include <SPI.h>
#include <mcp2515.h>

#include "../config.h"
#include "Canbus.h"

Canbus::Canbus(MCP2515 *mcp2515) {
    this->mcp2515 = mcp2515;

    lastReadStatusMsg1 = 0;
    lastReadStatusMsg2 = 0;
    transferId = 0;

    temperature = 0;
    milliCurrent = 0;
    milliVoltage = 0;
    rpm = 0;
}

void Canbus::parseCanMsg(struct can_frame *canMsg) {

    uint16_t dataTypeId = getDataTypeIdFromCanId(canMsg->can_id);

    if (
        dataTypeId != statusMsg1
        && dataTypeId != statusMsg2
    ) {
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
    lastReadStatusMsg2 = millis();
}

void Canbus::handleStatusMsg2(struct can_frame *canMsg) {
    if (canMsg->can_dlc != 6) {
        return;
    }

    temperature = getTemperatureFromPayload(canMsg->data);
    milliCurrent = getMiliCurrentFromPayload(canMsg->data);
    milliVoltage = getMiliVoltageFromPayload(canMsg->data);
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
    return (canId & 0x80) >> 15 == 1;
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

uint16_t Canbus::getMiliCurrentFromPayload(uint8_t *payload) {
    return (payload[3] << 8) | payload[2];
}

uint16_t Canbus::getMiliVoltageFromPayload(uint8_t *payload) {
    return (payload[1] << 8) | payload[0];
}

uint16_t Canbus::getRpmFromPayload(uint8_t *payload) {
    return (payload[1] << 8) | payload[0];
}

void Canbus::setLedColor(uint8_t color)
{
    struct can_frame canMsg;

    canMsg.can_id = ((uint32_t)0 << 24) // priority
        | ((uint32_t)setLedDataTypeId << 8)  // data type id
        | (uint32_t)escNodeId; // node id

    canMsg.can_dlc = 4;
    canMsg.data[0] = 0x00; // setLedOptionSave 0x01
    canMsg.data[1] = color;
    canMsg.data[2] = setLedBlinkOff;
    canMsg.data[3] = 0xC0 | (transferId & 31); // tail byte

    mcp2515->sendMessage(&canMsg);

    transferId += 1;
}

