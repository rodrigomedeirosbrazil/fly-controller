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

    // Fallback: route directly to classes (for backward compatibility)
    #if IS_TMOTOR
    uint16_t dataTypeId = CanUtils::getDataTypeIdFromCanId(canMsg->identifier);
    if (dataTypeId != 0 && isTmotorEscMessage(dataTypeId)) {
        tmotor.parseEscMessage(canMsg);
        return;
    }
    // Handle NodeStatus from ESC (not our own)
    if (dataTypeId == 341) {
        uint8_t sourceNodeId = CanUtils::getNodeIdFromCanId(canMsg->identifier);
        // Check if it's from ESC (not our own nodeId = 0x13)
        if (sourceNodeId != 0x13) {
            tmotor.handleNodeStatus(canMsg);
            return;
        }
    }
    #else
    uint16_t dataTypeId = CanUtils::getDataTypeIdFromCanId(canMsg->identifier);
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

