#ifndef Canbus_h
#define Canbus_h

#include <mcp2515.h>

// Forward declaration
class Hobbywing;
class Jkbms;

class Canbus
{
    public:
        void parseCanMsg(struct can_frame *canMsg);
        void printCanMsg(struct can_frame *canMsg);

    private:
        // Device type detection
        bool isHobbywingEscMessage(uint16_t dataTypeId);
        bool isJkbmsMessage(uint32_t canId);

        // CAN ID parsing methods
        uint16_t getDataTypeIdFromCanId(uint32_t canId);
        uint8_t getNodeIdFromCanId(uint32_t canId);
};

#endif
