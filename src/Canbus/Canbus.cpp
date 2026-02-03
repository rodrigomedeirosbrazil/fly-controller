#include <Arduino.h>
#include "Canbus.h"
#include "CanUtils.h"
#include "../config_controller.h"
#if USES_CAN_BUS
#include <driver/twai.h>
#include "../Telemetry/TelemetryProvider.h"
#include "../config.h"
#if IS_TMOTOR
#include "../Tmotor/Tmotor.h"
extern Tmotor tmotor;
#else
#include "../Hobbywing/Hobbywing.h"
extern Hobbywing hobbywing;
#endif
// telemetry is declared in config.cpp after TelemetryProvider is fully defined
extern TelemetryProvider* telemetry;
#endif



#if USES_CAN_BUS
void Canbus::parseCanMsg(twai_message_t *canMsg) {
    // Skip service frames - ESC telemetry comes as message frames, not service frames
    if (CanUtils::isServiceFrame(canMsg->identifier)) {
        return;
    }

    // Try to route through telemetry provider first
    if (telemetry && telemetry->handleCanMessage) {
        uint16_t dataTypeId = CanUtils::getDataTypeIdFromCanId(canMsg->identifier);

        // Check if this is a message for our ESC
        bool isEscMessage = false;
        #if IS_TMOTOR
        isEscMessage = isTmotorEscMessage(dataTypeId);
        #else
        isEscMessage = isHobbywingEscMessage(dataTypeId);
        #endif

        if (isEscMessage) {
            telemetry->handleCanMessage(canMsg);
            return;
        }
    }

    // Extract dataTypeId once for all checks
    uint16_t dataTypeId = CanUtils::getDataTypeIdFromCanId(canMsg->identifier);

    // Handle NodeStatus from ESC (not our own) - generic DroneCAN protocol
    if (dataTypeId == nodeStatusDataTypeId) {
        handleNodeStatus(canMsg);
        return;
    }

    // Fallback: route directly to classes (for backward compatibility)
    #if IS_TMOTOR
    if (dataTypeId != 0 && isTmotorEscMessage(dataTypeId)) {
        tmotor.parseEscMessage(canMsg);
        return;
    }
    #else
    if (dataTypeId != 0 && isHobbywingEscMessage(dataTypeId)) {
        hobbywing.parseEscMessage(canMsg);
        return;
    }
    #endif
}

void Canbus::printCanMsg(twai_message_t *canMsg) {
    if (CanUtils::isServiceFrame(canMsg->identifier)) {
        Serial.print("Service Frame - Service ID: ");
        Serial.print(CanUtils::getServiceTypeIdFromCanId(canMsg->identifier));
        Serial.print(" Node ID: ");
        Serial.println(CanUtils::getNodeIdFromCanId(canMsg->identifier));
    } else {
        Serial.print("Message Frame - Data Type ID: ");
        Serial.print(CanUtils::getDataTypeIdFromCanId(canMsg->identifier));
        Serial.print(" Node ID: ");
        Serial.println(CanUtils::getNodeIdFromCanId(canMsg->identifier));
    }
    Serial.print("CAN Frame: ID=0x");
    Serial.print(canMsg->identifier, HEX);
    Serial.print(" DLC=");
    Serial.print(canMsg->data_length_code);
    Serial.print(" Data=");
    for (uint8_t i = 0; i < canMsg->data_length_code; i++) {
        if (i > 0) Serial.print(" ");
        if (canMsg->data[i] < 0x10) Serial.print("0");
        Serial.print(canMsg->data[i], HEX);
    }
    Serial.println();
}
#endif

bool Canbus::isHobbywingEscMessage(uint16_t dataTypeId) {
    return (dataTypeId == 0x4E52 ||  // statusMsg1
            dataTypeId == 0x4E53 ||  // statusMsg2
            dataTypeId == 0x4E54 ||  // statusMsg3
            dataTypeId == 0x4E56);   // getEscIdRequestDataTypeId
}

bool Canbus::isTmotorEscMessage(uint16_t dataTypeId) {
    return (dataTypeId == 1030 ||  // RawCommand
            dataTypeId == 1033 ||  // ParamCfg
            dataTypeId == 1034 ||  // ESC_STATUS
            dataTypeId == 1039 ||  // PUSHCAN
            dataTypeId == 1332);   // ParamGet
}

void Canbus::handle() {
    // Route to the appropriate motor handler based on ESC type
    #if USES_CAN_BUS && IS_TMOTOR
    extern Tmotor tmotor;
    tmotor.handle();
    #endif
    #if USES_CAN_BUS && IS_HOBBYWING
    extern Hobbywing hobbywing;
    hobbywing.handle();
    #endif
}

void Canbus::sendNodeStatus() {
    if (millis() - lastNodeStatusSent < 1000) {
        return;
    }

    lastNodeStatusSent = millis();

    uint32_t uptimeSec = millis() / 1000;

    twai_message_t localCanMsg;
    memset(&localCanMsg, 0, sizeof(twai_message_t));  // Initialize all fields to zero

    uint32_t canId = 0;
    canId |= ((uint32_t)0 << 26);              // Priority
    canId |= ((uint32_t)nodeStatusDataTypeId << 8);      // DataType ID
    canId |= (uint32_t)(nodeId & 0xFF);        // Source node ID
    localCanMsg.identifier = canId;
    localCanMsg.extd = 1;  // Extended frame (29-bit)
    localCanMsg.rtr = 0;   // Data frame (not remote transmission request)

    localCanMsg.data[0] = (uint8_t)(uptimeSec & 0xFF);
    localCanMsg.data[1] = (uint8_t)((uptimeSec >> 8) & 0xFF);
    localCanMsg.data[2] = (uint8_t)((uptimeSec >> 16) & 0xFF);
    localCanMsg.data[3] = (uint8_t)((uptimeSec >> 24) & 0xFF);
    localCanMsg.data[4] = 0x00; // health=0 (OK), mode=0 (operational), sub_mode=0 (normal)
    localCanMsg.data[5] = 0x00; // LSB
    localCanMsg.data[6] = 0x00; // MSB

    // Tail byte (SOF=1, EOF=1, Toggle=0, Transfer ID)
    localCanMsg.data[7] = 0xC0 | (transferId & 0x1F);

    localCanMsg.data_length_code = 8;  // CAN 2.0 maximum

    twai_transmit(&localCanMsg, pdMS_TO_TICKS(100));
    transferId = (transferId + 1) & 0x1F;
}

void Canbus::handleNodeStatus(twai_message_t *canMsg) {
    // Extract Node ID from CAN ID
    uint8_t escNodeIdFromMsg = CanUtils::getNodeIdFromCanId(canMsg->identifier);

    // Ignore our own NodeStatus
    if (escNodeIdFromMsg == nodeId) {
        return;
    }

    // Store detected ESC Node ID
    if (escNodeId == 0) {
        escNodeId = escNodeIdFromMsg;
        Serial.print("[Canbus] Detected ESC at Node ID: ");
        Serial.println(escNodeId);
    } else if (escNodeId != escNodeIdFromMsg) {
        // ESC node ID changed (shouldn't happen, but handle it)
        escNodeId = escNodeIdFromMsg;
        Serial.print("[Canbus] ESC Node ID changed to: ");
        Serial.println(escNodeId);
    }
}

