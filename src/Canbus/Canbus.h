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

    private:
        // CAN bus node ID
        const uint8_t nodeId = 0x13;

        // Device type detection
        bool isHobbywingEscMessage(uint16_t dataTypeId);
        bool isTmotorEscMessage(uint16_t dataTypeId);
};

#endif
