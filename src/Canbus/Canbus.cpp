#include <Arduino.h>
#include <driver/twai.h>

#include "../config.h"
#include "Canbus.h"
#ifndef XAG
#ifdef T_MOTOR
#include "../Tmotor/Tmotor.h"
extern Tmotor tmotor;
#else
#include "../Hobbywing/Hobbywing.h"
extern Hobbywing hobbywing;
#endif
#endif



void Canbus::parseCanMsg(twai_message_t *canMsg) {
#ifndef XAG
    uint16_t dataTypeId = getDataTypeIdFromCanId(canMsg->identifier);

#ifdef T_MOTOR
    // Route T-Motor ESC messages to the Tmotor class
    if (isTmotorEscMessage(dataTypeId)) {
        tmotor.parseEscMessage(canMsg);
        return;
    }
#else
    // Route Hobbywing ESC messages to the Hobbywing class
    if (isHobbywingEscMessage(dataTypeId)) {
        hobbywing.parseEscMessage(canMsg);
        return;
    }
#endif

    // Print unknown messages for debugging
    printCanMsg(canMsg);
#endif
}

void Canbus::printCanMsg(twai_message_t *canMsg) {
    Serial.print("Data Type ID: ");
    Serial.print(getDataTypeIdFromCanId(canMsg->identifier), HEX);
    Serial.print(" Node ID: ");
    Serial.println(getNodeIdFromCanId(canMsg->identifier), HEX);
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

uint16_t Canbus::getDataTypeIdFromCanId(uint32_t canId) {
    return (canId >> 8) & 0xFFFF;
}

uint8_t Canbus::getNodeIdFromCanId(uint32_t canId) {
    return canId & 0x7F;
}

