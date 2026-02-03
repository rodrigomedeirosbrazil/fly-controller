#ifndef Canbus_h
#define Canbus_h

#include <driver/twai.h>

// Forward declaration
#if USES_CAN_BUS
#if IS_TMOTOR
class Tmotor;
#else
class Hobbywing;
#endif
#endif

class Canbus
{
    public:
        void parseCanMsg(twai_message_t *canMsg);
        void printCanMsg(twai_message_t *canMsg);
        void handle();
        uint8_t getNodeId() const { return nodeId; }
        uint8_t getEscNodeId() const { return escNodeId; }
        void sendNodeStatus();

    private:
        // CAN bus node ID
        const uint8_t nodeId = 0x13;
        const uint16_t nodeStatusDataTypeId = 341;

        // ESC detection
        uint8_t escNodeId = 0;  // Node ID of ESC detected via NodeStatus

        // NodeStatus sending control
        unsigned long lastNodeStatusSent = 0;
        uint8_t transferId = 0;

        // Device type detection
        bool isHobbywingEscMessage(uint16_t dataTypeId);
        bool isTmotorEscMessage(uint16_t dataTypeId);

        // NodeStatus handling (generic DroneCAN protocol)
        void handleNodeStatus(twai_message_t *canMsg);
};

#endif
