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
    // Debug: Print all incoming CAN messages
    Serial.print("[CAN] RX: ID=0x");
    Serial.print(canMsg->identifier, HEX);
    Serial.print(" DLC=");
    Serial.print(canMsg->data_length_code);

    if (CanUtils::isServiceFrame(canMsg->identifier)) {
        Serial.print(" [SERVICE] ServiceID=");
        Serial.print(CanUtils::getServiceTypeIdFromCanId(canMsg->identifier));
        Serial.print(" Req/Resp=");
        Serial.print(CanUtils::isRequestFrame(canMsg->identifier) ? "REQ" : "RESP");
        Serial.print(" SrcNode=");
        Serial.print(CanUtils::getNodeIdFromCanId(canMsg->identifier));
        Serial.print(" DstNode=");
        Serial.print(CanUtils::getDestNodeIdFromCanId(canMsg->identifier));
    } else {
        Serial.print(" [MESSAGE] DataTypeID=");
        uint16_t dataTypeId = CanUtils::getDataTypeIdFromCanId(canMsg->identifier);
        Serial.print(dataTypeId);
        // Identify common message types
        if (dataTypeId == 341) {
            Serial.print(" (NodeStatus)");
        } else if (dataTypeId == 1034) {
            Serial.print(" (ESC_STATUS)");
        } else if (dataTypeId == 1039) {
            Serial.print(" (PUSHCAN)");
        } else if (dataTypeId == 1030) {
            Serial.print(" (RawCommand)");
        }
        Serial.print(" SrcNode=");
        Serial.print(CanUtils::getNodeIdFromCanId(canMsg->identifier));
        Serial.print(" Priority=");
        Serial.print(CanUtils::getPriorityFromCanId(canMsg->identifier));
    }

    Serial.print(" Data=[");
    for (uint8_t i = 0; i < canMsg->data_length_code; i++) {
        if (i > 0) Serial.print(" ");
        if (canMsg->data[i] < 0x10) Serial.print("0");
        Serial.print(canMsg->data[i], HEX);
    }
    Serial.print("]");

    // Extract tail byte info if present
    if (canMsg->data_length_code > 0) {
        uint8_t tailByte = canMsg->data[canMsg->data_length_code - 1];
        Serial.print(" Tail=0x");
        if (tailByte < 0x10) Serial.print("0");
        Serial.print(tailByte, HEX);
        Serial.print("(SOF=");
        Serial.print(CanUtils::isStartOfFrame(tailByte) ? "1" : "0");
        Serial.print(" EOF=");
        Serial.print(CanUtils::isEndOfFrame(tailByte) ? "1" : "0");
        Serial.print(" TID=");
        Serial.print(CanUtils::getTransferId(tailByte));
        Serial.print(")");
    }
    Serial.println();

    // Skip service frames - ESC telemetry comes as message frames, not service frames
    if (CanUtils::isServiceFrame(canMsg->identifier)) {
        // Service frames are for RPC calls (GetNodeInfo, etc.), not telemetry
        Serial.println("[CAN] -> Service frame ignored (not telemetry)");
        return;
    }

    // Try to route through telemetry provider first
    if (telemetry && telemetry->handleCanMessage) {
        uint16_t dataTypeId = CanUtils::getDataTypeIdFromCanId(canMsg->identifier);

        // Check if this is a message for our ESC
        bool isEscMessage = false;
        #if IS_TMOTOR
        isEscMessage = isTmotorEscMessage(dataTypeId);
        Serial.print("[CAN] -> Checking Tmotor: DataTypeID=");
        Serial.print(dataTypeId);
        Serial.print(" isEscMsg=");
        Serial.println(isEscMessage ? "YES" : "NO");
        #else
        isEscMessage = isHobbywingEscMessage(dataTypeId);
        Serial.print("[CAN] -> Checking Hobbywing: DataTypeID=");
        Serial.print(dataTypeId);
        Serial.print(" isEscMsg=");
        Serial.println(isEscMessage ? "YES" : "NO");
        #endif

        if (isEscMessage) {
            Serial.println("[CAN] -> Routing to telemetry provider");
            telemetry->handleCanMessage(canMsg);
            return;
        }
    }

    // Fallback: route directly to classes (for backward compatibility)
    #if IS_TMOTOR
    uint16_t dataTypeId = CanUtils::getDataTypeIdFromCanId(canMsg->identifier);
    if (dataTypeId != 0 && isTmotorEscMessage(dataTypeId)) {
        Serial.println("[CAN] -> Routing to Tmotor::parseEscMessage (fallback)");
        tmotor.parseEscMessage(canMsg);
        return;
    }
    // Handle NodeStatus from ESC (not our own)
    if (dataTypeId == 341) {
        uint8_t sourceNodeId = CanUtils::getNodeIdFromCanId(canMsg->identifier);
        // Check if it's from ESC (not our own nodeId = 0x13)
        if (sourceNodeId != 0x13) {
            Serial.println("[CAN] -> Routing NodeStatus to Tmotor::handleNodeStatus");
            tmotor.handleNodeStatus(canMsg);
            return;
        }
    }
    #else
    uint16_t dataTypeId = CanUtils::getDataTypeIdFromCanId(canMsg->identifier);
    if (dataTypeId != 0 && isHobbywingEscMessage(dataTypeId)) {
        Serial.println("[CAN] -> Routing to Hobbywing::parseEscMessage (fallback)");
        hobbywing.parseEscMessage(canMsg);
        return;
    }
    #endif

    // Print unknown messages for debugging
    Serial.println("[CAN] -> Unknown message (not processed)");
    printCanMsg(canMsg);
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

