#include <Arduino.h>
#include <SPI.h>
#include <mcp2515.h>

#include "../config.h"
#include "Jkbms.h"

Jkbms::Jkbms() {
    resetData();
}

void Jkbms::parseJkbmsMessage(twai_message_t *canMsg) {
    if (!isJkbmsMessage(canMsg->identifier)) {
        return;
    }

    lastUpdateTime = millis();

    switch (canMsg->identifier) {
        case JK_CAN_ID_BASIC_INFO:
            parseBasicInfoMessage(canMsg);
            break;

        case JK_CAN_ID_CELL_VOLTAGES:
            parseCellVoltageMessage(canMsg);
            break;

        case JK_CAN_ID_TEMPERATURE:
            parseTemperatureMessage(canMsg);
            break;

        case JK_CAN_ID_PROTECTION:
            parseProtectionMessage(canMsg);
            break;

        default:
            // Unknown JK BMS message
            break;
    }
}

void Jkbms::parseBasicInfoMessage(twai_message_t *canMsg) {
    if (canMsg->data_length_code < 8) return;

    // Parse basic battery information
    // Byte 0-1: Total voltage (0.01V resolution)
    totalVoltage = ((canMsg->data[0] << 8) | canMsg->data[1]) * 0.01f;

    // Byte 2-3: Current (0.01A resolution, signed)
    int16_t currentRaw = (canMsg->data[2] << 8) | canMsg->data[3];
    current = currentRaw * 0.01f;

    // Byte 4: SOC (0-100%)
    soc = canMsg->data[4];

    // Byte 5: SOH (0-100%)
    soh = canMsg->data[5];

    // Byte 6-7: Cycle count
    cycleCount = (canMsg->data[6] << 8) | canMsg->data[7];

    // Update charging/discharging status
    charging = (current > 0.1f);
    discharging = (current < -0.1f);
}

void Jkbms::parseCellVoltageMessage(twai_message_t *canMsg) {
    if (canMsg->data_length_code < 8) return;

    // Parse cell voltages (up to 4 cells per message)
    // Each cell voltage is 2 bytes with 0.001V resolution
    uint8_t cellsInMessage = (canMsg->data_length_code - 1) / 2;  // First byte is cell count

    if (cellsInMessage > 4) cellsInMessage = 4;  // Max 4 cells per message

    uint8_t startCell = canMsg->data[0];  // Starting cell index

    for (uint8_t i = 0; i < cellsInMessage; i++) {
        uint8_t cellIndex = startCell + i;
        if (cellIndex < 24) {  // Max 24 cells
            uint16_t voltageRaw = (canMsg->data[1 + i*2] << 8) | canMsg->data[2 + i*2];
            cellVoltages[cellIndex] = voltageRaw * 0.001f;

            if (cellIndex == 0) {
                minCellVoltage = cellVoltages[cellIndex];
                maxCellVoltage = cellVoltages[cellIndex];
                cellCount = 1;
            } else {
                if (cellVoltages[cellIndex] < minCellVoltage) {
                    minCellVoltage = cellVoltages[cellIndex];
                }
                if (cellVoltages[cellIndex] > maxCellVoltage) {
                    maxCellVoltage = cellVoltages[cellIndex];
                }
                if (cellIndex >= cellCount) {
                    cellCount = cellIndex + 1;
                }
            }
        }
    }
}

void Jkbms::parseTemperatureMessage(twai_message_t *canMsg) {
    if (canMsg->data_length_code < 4) return;

    // Parse temperature sensors
    // Byte 0-1: Temperature sensor 1 (0.1°C resolution, signed)
    int16_t temp1Raw = (canMsg->data[0] << 8) | canMsg->data[1];
    temperature1 = temp1Raw * 0.1f;

    // Byte 2-3: Temperature sensor 2 (0.1°C resolution, signed)
    int16_t temp2Raw = (canMsg->data[2] << 8) | canMsg->data[3];
    temperature2 = temp2Raw * 0.1f;
}

void Jkbms::parseProtectionMessage(twai_message_t *canMsg) {
    if (canMsg->data_length_code < 2) return;

    // Parse protection flags
    protectionFlags = (canMsg->data[0] << 8) | canMsg->data[1];
    protectionActive = (protectionFlags != 0);
}

bool Jkbms::isJkbmsMessage(uint32_t canId) {
    return (canId >= JK_CAN_ID_BASIC_INFO && canId <= JK_CAN_ID_PROTECTION);
}

float Jkbms::getCellVoltage(uint8_t cellIndex) const {
    if (cellIndex >= cellCount) {
        return 0.0f;
    }
    return cellVoltages[cellIndex];
}

bool Jkbms::isDataFresh() const {
    return (millis() - lastUpdateTime) < DATA_TIMEOUT_MS;
}

void Jkbms::updateCellVoltageStats() {
    if (cellCount == 0) return;

    minCellVoltage = cellVoltages[0];
    maxCellVoltage = cellVoltages[0];

    for (uint8_t i = 1; i < cellCount; i++) {
        if (cellVoltages[i] < minCellVoltage) {
            minCellVoltage = cellVoltages[i];
        }
        if (cellVoltages[i] > maxCellVoltage) {
            maxCellVoltage = cellVoltages[i];
        }
    }
}

void Jkbms::resetData() {
    totalVoltage = 0.0f;
    current = 0.0f;
    temperature1 = 0.0f;
    temperature2 = 0.0f;
    soc = 0;
    soh = 0;
    cycleCount = 0;

    cellCount = 0;
    for (uint8_t i = 0; i < 24; i++) {
        cellVoltages[i] = 0.0f;
    }
    minCellVoltage = 0.0f;
    maxCellVoltage = 0.0f;

    charging = false;
    discharging = false;
    protectionActive = false;
    protectionFlags = 0;

    lastUpdateTime = 0;
}

void Jkbms::printStatus() const {
    Serial.println("=== JK BMS Status ===");
    Serial.print("Total Voltage: ");
    Serial.print(totalVoltage, 2);
    Serial.println(" V");

    Serial.print("Current: ");
    Serial.print(current, 2);
    Serial.println(" A");

    Serial.print("SOC: ");
    Serial.print(soc);
    Serial.println(" %");

    Serial.print("SOH: ");
    Serial.print(soh);
    Serial.println(" %");

    Serial.print("Temperature 1: ");
    Serial.print(temperature1, 1);
    Serial.println(" °C");

    Serial.print("Temperature 2: ");
    Serial.print(temperature2, 1);
    Serial.println(" °C");

    Serial.print("Cell Count: ");
    Serial.println(cellCount);

    Serial.print("Min Cell Voltage: ");
    Serial.print(minCellVoltage, 3);
    Serial.println(" V");

    Serial.print("Max Cell Voltage: ");
    Serial.print(maxCellVoltage, 3);
    Serial.println(" V");

    Serial.print("Charging: ");
    Serial.println(charging ? "Yes" : "No");

    Serial.print("Discharging: ");
    Serial.println(discharging ? "Yes" : "No");

    Serial.print("Protection Active: ");
    Serial.println(protectionActive ? "Yes" : "No");

    Serial.print("Data Fresh: ");
    Serial.println(isDataFresh() ? "Yes" : "No");

    Serial.print("Last Update: ");
    Serial.print(millis() - lastUpdateTime);
    Serial.println(" ms ago");

    Serial.println("=====================");
}
