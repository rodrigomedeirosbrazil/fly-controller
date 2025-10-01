#include <Arduino.h>
#include <driver/twai.h>

#include "../config.h"
#include "Canbus.h"
#include "../Hobbywing/Hobbywing.h"
#include "../Jkbms/Jkbms.h"

extern Hobbywing hobbywing;
extern Jkbms jkbms;



void Canbus::parseCanMsg(twai_message_t *canMsg) {
    uint16_t dataTypeId = getDataTypeIdFromCanId(canMsg->identifier);

    // Route JK BMS messages to the Jkbms class
    if (isJkbmsMessage(canMsg->identifier)) {
        jkbms.parseJkbmsMessage(canMsg);
        return;
    }

    // Route Hobbywing ESC messages to the Hobbywing class
    if (isHobbywingEscMessage(dataTypeId)) {
        hobbywing.parseEscMessage(canMsg);
        return;
    }

    // Print unknown messages for debugging
    printCanMsg(canMsg);
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

bool Canbus::isJkbmsMessage(uint32_t canId) {
    return (canId >= 0x100 && canId <= 0x103);
}

uint16_t Canbus::getDataTypeIdFromCanId(uint32_t canId) {
    return (canId >> 8) & 0xFFFF;
}

uint8_t Canbus::getNodeIdFromCanId(uint32_t canId) {
    return canId & 0x7F;
}

