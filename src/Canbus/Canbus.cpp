#include <Arduino.h>
#include <SPI.h>
#include <mcp2515.h>

#include "Canbus.h"

Canbus::Canbus() {
    mcp2515.reset();
    mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ);
    mcp2515.setNormalMode();

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
