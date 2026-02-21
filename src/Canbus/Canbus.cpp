#include <Arduino.h>
#include "Canbus.h"
#include "CanUtils.h"
#include "../config_controller.h"
#if USES_CAN_BUS
#include <driver/twai.h>
#include <string.h>

bool Canbus::receive(twai_message_t *outMsg) {
    twai_message_t msg;
    if (twai_receive(&msg, pdMS_TO_TICKS(0)) != ESP_OK) {
        return false;
    }

    // Handle service frames (e.g., GetNodeInfo requests)
    if (CanUtils::isServiceFrame(msg.identifier)) {
        uint16_t serviceTypeId = CanUtils::getServiceTypeIdFromCanId(msg.identifier);
        uint8_t destNodeId = CanUtils::getDestNodeIdFromCanId(msg.identifier);

        if (destNodeId == nodeId && serviceTypeId == 1 && CanUtils::isRequestFrame(msg.identifier)) {
            handleGetNodeInfoRequest(&msg);
        }
        return false;  // Frame consumed
    }

    // Handle NodeStatus - generic DroneCAN protocol
    uint16_t dataTypeId = CanUtils::getDataTypeIdFromCanId(msg.identifier);
    if (dataTypeId == nodeStatusDataTypeId) {
        handleNodeStatus(&msg);
        return false;  // Frame consumed
    }

    // Pass frame to caller for ESC processing
    memcpy(outMsg, &msg, sizeof(twai_message_t));
    return true;
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

void Canbus::handleGetNodeInfoRequest(twai_message_t *canMsg) {
    // Extract source Node ID (who is requesting)
    uint8_t requestorNodeId = CanUtils::getNodeIdFromCanId(canMsg->identifier);

    // Extract Transfer ID from tail byte
    uint8_t transferId = 0;
    if (canMsg->data_length_code > 0) {
        uint8_t tailByte = canMsg->data[canMsg->data_length_code - 1];
        transferId = CanUtils::getTransferId(tailByte);
    }

    Serial.print("[Canbus] GetNodeInfo request from Node ID: ");
    Serial.println(requestorNodeId);

    // Send response
    sendGetNodeInfoResponse(requestorNodeId, transferId);
}

void Canbus::sendGetNodeInfoResponse(uint8_t requestorNodeId, uint8_t transferId) {
    twai_message_t localCanMsg;
    memset(&localCanMsg, 0, sizeof(twai_message_t));

    // Device information (minimal implementation - fits in single CAN frame)
    // Using shorter identifiers to fit in 7 bytes of payload (8th byte is tail)
    const char* deviceName = "fly-ctrl";  // Truncated to fit
    const char* softwareVersion = "1.0";   // Short version

    uint8_t nameLen = strlen(deviceName);
    uint8_t versionLen = strlen(softwareVersion);

    // Build CAN ID for response
    // DroneCAN service frame format for GetNodeInfo response:
    // [28:26] Priority (3 bits) = 0
    // [25:16] Service Type ID (10 bits) = 1 (GetNodeInfo)
    // [15]    Request/Response (1 bit) = 0 (Response)
    // [14:8]  Destination Node ID (7 bits) = requestorNodeId
    // [7]     Service/Message (1 bit) = 1 (Service)
    // [6:0]   Source Node ID (7 bits) = nodeId (0x13)
    uint32_t canId = 0;
    canId |= ((uint32_t)0 << 26);              // Priority = 0
    canId |= ((uint32_t)1 << 16);              // Service Type ID = 1 (GetNodeInfo)
    canId |= (0U << 15);                       // Response (not request)
    canId |= ((uint32_t)(requestorNodeId & 0x7F)) << 8;  // Destination Node ID
    canId |= (1U << 7);                        // Service frame
    canId |= (uint32_t)(nodeId & 0x7F);        // Source Node ID

    localCanMsg.identifier = canId;
    localCanMsg.extd = 1;  // Extended frame (29-bit)
    localCanMsg.rtr = 0;   // Data frame

    // Build payload: device name (null-terminated) + software version (null-terminated)
    // Maximum 7 bytes for payload (8th byte reserved for tail)
    uint8_t payloadIdx = 0;

    // Copy device name (with null terminator)
    for (uint8_t i = 0; i <= nameLen && payloadIdx < 7; i++) {
        localCanMsg.data[payloadIdx++] = deviceName[i];
    }

    // Copy software version (with null terminator) if space allows
    if (payloadIdx + versionLen + 1 <= 7) {
        for (uint8_t i = 0; i <= versionLen && payloadIdx < 7; i++) {
            localCanMsg.data[payloadIdx++] = softwareVersion[i];
        }
    }

    // Tail byte (SOF=1, EOF=1, Toggle=0, Transfer ID)
    localCanMsg.data[payloadIdx] = 0xC0 | (transferId & 0x1F);

    localCanMsg.data_length_code = payloadIdx + 1;

    Serial.print("[Canbus] TX: GetNodeInfo response to Node ");
    Serial.print(requestorNodeId);
    Serial.print(" - Name: ");
    Serial.print(deviceName);
    Serial.print(", Version: ");
    Serial.println(softwareVersion);

    esp_err_t tx_result = twai_transmit(&localCanMsg, pdMS_TO_TICKS(100));
    if (tx_result != ESP_OK) {
        Serial.print("[Canbus] ERROR: Failed to send GetNodeInfo response - Code: ");
        Serial.println(tx_result);
    }
}

void Canbus::requestNodeInfo(uint8_t targetNodeId) {
    Serial.print("[Canbus] requestNodeInfo() called for Node ID: ");
    Serial.println(targetNodeId);

    // Send GetNodeInfo service request to discover node capabilities
    // Service ID for GetNodeInfo is 1 (uavcan.protocol.GetNodeInfo)
    twai_message_t localCanMsg;
    memset(&localCanMsg, 0, sizeof(twai_message_t));  // Initialize all fields to zero

    // DroneCAN service frame format for GetNodeInfo request:
    // [28:26] Priority (3 bits) = 0
    // [25:16] Service Type ID (10 bits) = 1 (GetNodeInfo)
    // [15]    Request/Response (1 bit) = 1 (Request)
    // [14:8]  Destination Node ID (7 bits) = targetNodeId
    // [7]     Service/Message (1 bit) = 1 (Service)
    // [6:0]   Source Node ID (7 bits) = nodeId (0x13)

    uint32_t canId = 0;
    canId |= ((uint32_t)0 << 26);              // Priority = 0
    canId |= ((uint32_t)1 << 16);              // Service Type ID = 1 (GetNodeInfo)
    canId |= (1U << 15);                       // Request (not response)
    canId |= ((uint32_t)(targetNodeId & 0x7F)) << 8;  // Destination Node ID
    canId |= (1U << 7);                        // Service frame
    canId |= (uint32_t)(nodeId & 0x7F);        // Source Node ID

    localCanMsg.identifier = canId;
    localCanMsg.extd = 1;  // Extended frame (29-bit)
    localCanMsg.rtr = 0;   // Data frame (not remote transmission request)

    // GetNodeInfo request has no payload (empty request)
    localCanMsg.data[0] = 0xC0 | (transferId & 0x1F);  // Tail byte (SOF=1, EOF=1, TID)
    localCanMsg.data_length_code = 1;

    Serial.print("[Canbus] TX: GetNodeInfo request to Node ");
    Serial.print(targetNodeId);
    Serial.print(" - CAN ID=0x");
    Serial.print(canId, HEX);
    Serial.print(" DLC=");
    Serial.print(localCanMsg.data_length_code);
    Serial.print(" Data=[");
    for (uint8_t i = 0; i < localCanMsg.data_length_code; i++) {
        if (i > 0) Serial.print(" ");
        if (localCanMsg.data[i] < 0x10) Serial.print("0");
        Serial.print(localCanMsg.data[i], HEX);
    }
    Serial.print("]");

    // Check driver status
    twai_status_info_t status_info;
    twai_get_status_info(&status_info);
    Serial.print(" TXErr=");
    Serial.print(status_info.tx_error_counter);

    esp_err_t tx_result = twai_transmit(&localCanMsg, pdMS_TO_TICKS(100));  // Increased timeout
    if (tx_result == ESP_OK) {
        Serial.println(" - OK");
    } else {
        Serial.print(" - ERROR: ");
        if (tx_result == ESP_ERR_INVALID_STATE) {
            Serial.println("INVALID_STATE");
        } else if (tx_result == ESP_ERR_INVALID_ARG) {
            Serial.println("INVALID_ARG");
        } else if (tx_result == ESP_ERR_TIMEOUT) {
            Serial.println("TIMEOUT");
        } else {
            Serial.print("Code ");
            Serial.println(tx_result);
        }
    }
    transferId = (transferId + 1) & 0x1F;
}

