#include <Arduino.h>
#include <SPI.h>
#include <mcp2515.h>

#include "Hobbywing.h"

Hobbywing::Hobbywing() {
    lastReadStatusMsg1 = 0;
    lastReadStatusMsg2 = 0;

    temperature = 0;
    deciCurrent = 0;
    deciVoltage = 0;
    rpm = 0;
    isCcwDirection = false;
}

void Hobbywing::parseEscMessage(struct can_frame *canMsg) {
    // Extract DataType ID from CAN ID (assuming DroneCAN format)
    uint16_t dataTypeId = (canMsg->can_id >> 8) & 0xFFFF;

    // Check if this is a Hobbywing ESC message
    if (dataTypeId != statusMsg1 && dataTypeId != statusMsg2 && dataTypeId != statusMsg3) {
        return; // Not a Hobbywing ESC message
    }

    uint8_t tailByte = getTailByteFromPayload(canMsg->data, canMsg->can_dlc);

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

void Hobbywing::handleStatusMsg1(struct can_frame *canMsg) {
    if (canMsg->can_dlc != 7) {
        return;
    }

    rpm = getRpmFromPayload(canMsg->data);
    isCcwDirection = getDirectionCCWFromPayload(canMsg->data);

    lastReadStatusMsg2 = millis();
}

void Hobbywing::handleStatusMsg2(struct can_frame *canMsg) {
    if (canMsg->can_dlc != 6) {
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
