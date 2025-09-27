#include <Arduino.h>
#include <SPI.h>
#include <mcp2515.h>

#include "../config.h"
#include "Canbus.h"
#include "../Hobbywing/Hobbywing.h"

extern Hobbywing hobbywing;



void Canbus::parseCanMsg(struct can_frame *canMsg) {
    uint16_t dataTypeId = getDataTypeIdFromCanId(canMsg->can_id);

    // Route Hobbywing ESC messages to the Hobbywing class
    if (isHobbywingEscMessage(dataTypeId)) {
        hobbywing.parseEscMessage(canMsg);
        return;
    }

    // Print unknown messages for debugging
    printCanMsg(canMsg);
}

void Canbus::printCanMsg(struct can_frame *canMsg) {
    Serial.print("Data Type ID: ");
    Serial.print(getDataTypeIdFromCanId(canMsg->can_id), HEX);
    Serial.print(" Node ID: ");
    Serial.println(getNodeIdFromCanId(canMsg->can_id), HEX);
    Serial.print("CAN Frame: ID=0x");
    Serial.print(canMsg->can_id, HEX);
    Serial.print(" DLC=");
    Serial.print(canMsg->can_dlc);
    Serial.print(" Data=");
    for (uint8_t i = 0; i < canMsg->can_dlc; i++) {
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

uint16_t Canbus::getDataTypeIdFromCanId(uint32_t canId) {
    return (canId >> 8) & 0xFFFF;
}

uint8_t Canbus::getNodeIdFromCanId(uint32_t canId) {
    return canId & 0x7F;
}

