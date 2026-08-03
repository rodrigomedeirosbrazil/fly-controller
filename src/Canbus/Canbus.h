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

        /**
         * Drives the TWAI driver back to RUNNING after a bus-off.
         *
         * A CAN node that transmits with nothing to acknowledge it (the cable
         * unplugged, the ESC unpowered) accumulates 8 TEC counts per failed
         * attempt, and the driver retransmits automatically — so this
         * controller, sending RawCommand at 400 Hz, crosses the 256-count
         * bus-off threshold within milliseconds of the bus going away.
         *
         * Bus-off is latching by design: the peripheral detaches from the bus
         * and stops BOTH transmitting and receiving until software explicitly
         * initiates recovery. Nothing did, so unplugging the CAN cable killed
         * the bus permanently — reconnecting it never brought telemetry back,
         * because the driver had already given up.
         *
         * Must be called every loop iteration. Cheap: one status read.
         */
        void handleBusRecovery();

        /** True while the bus is off or being recovered (no traffic possible). */
        bool isBusRecovering() const { return busRecovering; }

    private:
        // CAN bus node ID
        const uint8_t nodeId = 0x13;
        const uint16_t nodeStatusDataTypeId = 341;

        // ESC detection
        uint8_t escNodeId = 0;  // Node ID of ESC detected via NodeStatus

        // NodeStatus sending control
        unsigned long lastNodeStatusSent = 0;
        uint8_t transferId = 0;

        // Bus-off recovery state
        bool busRecovering = false;

        // NodeStatus handling (generic DroneCAN protocol)
        void handleNodeStatus(twai_message_t *canMsg);

        // GetNodeInfo service handling
        void handleGetNodeInfoRequest(twai_message_t *canMsg);
        void sendGetNodeInfoResponse(uint8_t requestorNodeId, uint8_t transferId);
};

#endif
