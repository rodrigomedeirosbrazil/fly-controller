#include <Arduino.h>
#include <SPI.h>
#include <mcp2515.h>

#include "Canbus.h"

#define CAN_EFF_FLAG 0x80000000U /* EFF/SFF is set in the MSB */

    #if SERIAL_DEBUG
        Serial.println("------- CAN Read ----------");
        Serial.println("ID  DLC   DATA");
    #endif
}

void Canbus::tick() {
    #if SERIAL_DEBUG
    if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
        // canMsg.can_id
        // canMsg.can_dlc = data length
        // canMsg.data

        Serial.print(canMsg.can_id, HEX); // print ID
        Serial.print(" "); 
        Serial.print(canMsg.can_dlc, HEX); // print DLC
        Serial.print(" ");
        
        for (int i = 0; i<canMsg.can_dlc; i++)  {  // print the data
            Serial.print(canMsg.data[i],HEX);
            Serial.print(" ");
        }

        Serial.println();
    }
    #endif
}

uint8_t getPriorityFromCanId(uint32_t canId) {
    return (canId >> 24) & 0xFF;
}

uint16_t getDataTypeIdFromCanId(uint32_t canId) {
    return (canId >> 8) & 0xFFFF;
}

uint8_t getNodeIdFromCanId(uint32_t canId) {
    return canId & 0xFF;
}

uint32_t createCanId(uint8_t priority, uint16_t dataTypeId, uint8_t nodeId) {
    return ((uint32_t)priority << 24) | ((uint32_t)dataTypeId << 8) | (uint32_t)nodeId;
}
