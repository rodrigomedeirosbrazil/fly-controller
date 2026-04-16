#include <Arduino.h>
#include <driver/twai.h>

#include "../config.h"
#include "HobbywingCan.h"
#include "../Canbus/CanUtils.h"

namespace {
// Static payload parser functions for Hobbywing CAN messages
static uint8_t parseTemperatureFromPayload(uint8_t *payload) {
    return payload[HobbywingCan::TEMP_OFFSET];
}

static uint32_t parseBatteryCurrentFromPayload(uint8_t *payload) {
    uint16_t deciCurrent = (payload[HobbywingCan::CURRENT_HIGH_BYTE_OFFSET] << 8) |
                          payload[HobbywingCan::CURRENT_LOW_BYTE_OFFSET];
    return (uint32_t)deciCurrent * 100;
}

static uint16_t parseBatteryVoltageMilliVoltsFromPayload(uint8_t *payload) {
    uint16_t deciVolts = (payload[HobbywingCan::VOLTAGE_HIGH_BYTE_OFFSET] << 8) |
                        payload[HobbywingCan::VOLTAGE_LOW_BYTE_OFFSET];
    return (uint16_t)(deciVolts * 100);
}

static uint16_t parseRpmFromPayload(uint8_t *payload) {
    return (payload[HobbywingCan::RPM_HIGH_BYTE_OFFSET] << 8) |
           payload[HobbywingCan::RPM_LOW_BYTE_OFFSET];
}

static bool parseDirectionCCWFromPayload(uint8_t *payload) {
    return (payload[HobbywingCan::DIRECTION_OFFSET] & HobbywingCan::DIRECTION_MASK) >> 7 == 1;
}

static uint8_t parseEscThrottleIdFromPayload(uint8_t *payload) {
    return payload[HobbywingCan::THROTTLE_ID_OFFSET];
}
} // namespace

HobbywingCan::HobbywingCan() {
    lastReadStatusMsg1 = 0;
    lastReadStatusMsg2 = 0;
    transferId = 0;

    temperature = 0;
    batteryCurrentMilliAmps = 0;
    batteryVoltageMilliVolts = 0;
    rpm = 0;
    isCcwDirection = false;
}

void HobbywingCan::parseEscMessage(twai_message_t *canMsg) {
    uint16_t dataTypeId = CanUtils::getDataTypeIdFromCanId(canMsg->identifier);

    if (dataTypeId == getEscIdRequestDataTypeId) {
        handleGetEscIdResponse(canMsg);
        return;
    }

    if (dataTypeId != statusMsg1 && dataTypeId != statusMsg2 && dataTypeId != statusMsg3) {
        return;
    }

    uint8_t tailByte = CanUtils::getTailByteFromPayload(canMsg->data, canMsg->data_length_code);

    if (!CanUtils::isStartOfFrame(tailByte) && !CanUtils::isEndOfFrame(tailByte) && CanUtils::isToggleFrame(tailByte)) {
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

bool HobbywingCan::hasTelemetry() {
    return lastReadStatusMsg1 != 0
        && lastReadStatusMsg2 != 0
        && millis() - lastReadStatusMsg1 < 1000
        && millis() - lastReadStatusMsg2 < 1000;
}

bool HobbywingCan::isReady() {
    extern Canbus canbus;
    return canbus.getEscNodeId() != 0;
}

void HobbywingCan::handleStatusMsg1(twai_message_t *canMsg) {
    if (canMsg->data_length_code != STATUS_MSG1_FRAME_LENGTH) {
        return;
    }

    rpm = parseRpmFromPayload(canMsg->data);
    isCcwDirection = parseDirectionCCWFromPayload(canMsg->data);

    lastReadStatusMsg2 = millis();
}

void HobbywingCan::handleStatusMsg2(twai_message_t *canMsg) {
    if (canMsg->data_length_code != STATUS_MSG2_FRAME_LENGTH) {
        return;
    }

    temperature = parseTemperatureFromPayload(canMsg->data);
    batteryCurrentMilliAmps = parseBatteryCurrentFromPayload(canMsg->data);
    batteryVoltageMilliVolts = parseBatteryVoltageMilliVoltsFromPayload(canMsg->data);
    lastReadStatusMsg1 = millis();
}


void HobbywingCan::setLedColor(uint8_t color, uint8_t blink) {
    uint8_t data[3];
    data[0] = 0x00;
    data[1] = color;
    data[2] = blink;

    extern Canbus canbus;
    uint8_t targetNodeId = (canbus.getEscNodeId() != 0) ? canbus.getEscNodeId() : escNodeId;
    sendMessage(0x00, 0xd4, targetNodeId, data, 3);
}

void HobbywingCan::setDirection(bool isCcw) {
    uint8_t data[1] = { static_cast<uint8_t>(isCcw ? 0x00 : 0x01) };

    extern Canbus canbus;
    uint8_t targetNodeId = (canbus.getEscNodeId() != 0) ? canbus.getEscNodeId() : escNodeId;
    sendMessage(0x00, 0xD5, targetNodeId, data, 1);
}

void HobbywingCan::setThrottleSource(uint8_t source) {
    uint8_t data[1] = { source };

    extern Canbus canbus;
    uint8_t targetNodeId = (canbus.getEscNodeId() != 0) ? canbus.getEscNodeId() : escNodeId;
    sendMessage(0x00, 0xD7, targetNodeId, data, 1);
}

void HobbywingCan::setRawThrottle(int16_t throttle) {
    twai_message_t localCanMsg;

    extern Canbus canbus;

    uint32_t dataTypeId = 1030;
    uint32_t canId = 0;
    canId |= ((uint32_t)0 << 26);
    canId |= (dataTypeId << 8);
    canId |= (canbus.getNodeId() & 0xFF);
    localCanMsg.identifier = canId;
    localCanMsg.extd = 1;

    uint8_t throttleArraySize = escThrottleId + 1;
    uint8_t byteIndex = 0;

    for (uint8_t i = 0; i < throttleArraySize; i++) {
        int16_t value = (i == escThrottleId) ? throttle : 0;
        localCanMsg.data[byteIndex++] = value & 0xFF;
        localCanMsg.data[byteIndex++] = (value >> 8) & 0xFF;
    }

    localCanMsg.data[byteIndex++] = 0xC0 | (transferId & 0x1F);
    localCanMsg.data_length_code = byteIndex;

    twai_transmit(&localCanMsg, pdMS_TO_TICKS(10));
    transferId++;
}

void HobbywingCan::requestEscId() {
    extern Canbus canbus;

    twai_message_t localCanMsg;
    uint32_t dataTypeId = 20013;
    uint32_t canId = 0;
    canId |= ((uint32_t)0 << 26);
    canId |= (dataTypeId << 8);
    canId |= (canbus.getNodeId() & 0xFF);
    localCanMsg.identifier = canId;
    localCanMsg.extd = 1;

    uint8_t i = 0;
    localCanMsg.data[i++] = 0xC0 | (transferId & 0x1F);
    localCanMsg.data_length_code = i;

    DEBUG_PRINTLN("Sending GetEscID as broadcast message");
    twai_transmit(&localCanMsg, pdMS_TO_TICKS(10));
    transferId++;
}

void HobbywingCan::handleGetEscIdResponse(twai_message_t *canMsg) {
    if (canMsg->data_length_code < 7) {
        DEBUG_PRINTLN("WARN: GetEscID response payload too short.");
        return;
    }

    uint8_t escThrottleIdReceived = parseEscThrottleIdFromPayload(canMsg->data);
    uint8_t sourceNodeId = CanUtils::getNodeIdFromCanId(canMsg->identifier);
    (void)escThrottleIdReceived; // only used in DEBUG_PRINT
    (void)sourceNodeId;          // only used in DEBUG_PRINT

    DEBUG_PRINT("Received GetEscID response from Node ID: ");
    DEBUG_PRINT_HEX(sourceNodeId, HEX);
    DEBUG_PRINT(" with Throttle ID: ");
    DEBUG_PRINTLN(escThrottleIdReceived);
}

void HobbywingCan::sendMessage(uint8_t priority, uint16_t serviceTypeId, uint8_t destNodeId,
                               uint8_t *payload, uint8_t payloadLength) {
    extern Canbus canbus;

    twai_message_t localCanMsg;

    uint32_t canId = 0;
    canId |= ((uint32_t)(priority & 0x07)) << 26;
    canId |= ((uint32_t)(serviceTypeId & 0x03FF)) << 16;
    canId |= (1U << 15);
    canId |= ((uint32_t)(destNodeId & 0x7F)) << 8;
    canId |= (1U << 7);
    canId |= (uint32_t)(canbus.getNodeId() & 0x7F);

    localCanMsg.identifier = canId;
    localCanMsg.extd = 1;

    localCanMsg.data_length_code = payloadLength + 1;
    for (uint8_t i = 0; i < payloadLength; i++) {
        localCanMsg.data[i] = payload[i];
    }

    localCanMsg.data[payloadLength] = 0xC0 | (transferId & 0x1F);

    twai_transmit(&localCanMsg, pdMS_TO_TICKS(10));

    transferId++;
}

void HobbywingCan::handle() {
}
