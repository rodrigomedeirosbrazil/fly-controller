#include <Arduino.h>
#include <SPI.h>
#include <mcp2515.h>

#include "Canbus.h"

Canbus::Canbus() {
    mcp2515.reset();
    mcp2515.setBitrate(CAN_500KBPS);
    mcp2515.setNormalMode();
}

void Canbus::tick() {
    if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
        // canMsg.can_id
        // canMsg.can_dlc = data length
        // canMsg.data
    }
}
