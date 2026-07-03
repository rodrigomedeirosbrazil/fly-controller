#ifndef Canbus_h
#define Canbus_h

#include <driver/twai.h>

class Canbus
{
    public:
        /**
         * Non-blocking receive. Returns true if a frame was received for caller to process (ESC data).
         * Returns false if no frame, or frame was consumed internally (NodeStatus, GetNodeInfo).
         */
        bool receive(twai_message_t *outMsg);
        void printCanMsg(twai_message_t *canMsg);
        uint8_t getNodeId() const { return nodeId; }
        uint8_t getEscNodeId() const { return escNodeId; }
        void sendNodeStatus();
        void requestNodeInfo(uint8_t targetNodeId);  // Request GetNodeInfo from a specific node

    private:
        // CAN bus node ID
        const uint8_t nodeId = 0x13;
        const uint16_t nodeStatusDataTypeId = 341;

        // ESC detection
        uint8_t escNodeId = 0;  // Node ID of ESC detected via NodeStatus

        // NodeStatus sending control
        unsigned long lastNodeStatusSent = 0;
        uint8_t transferId = 0;

        // NodeStatus handling (generic DroneCAN protocol)
        void handleNodeStatus(twai_message_t *canMsg);

        // GetNodeInfo service handling
        void handleGetNodeInfoRequest(twai_message_t *canMsg);
        void sendGetNodeInfoResponse(uint8_t requestorNodeId, uint8_t transferId);
};

#endif
