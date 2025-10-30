#ifndef Canbus_h
#define Canbus_h

#include <driver/twai.h>

// Forward declaration
class Hobbywing;

class Canbus
{
    public:
        void parseCanMsg(twai_message_t *canMsg);
        void printCanMsg(twai_message_t *canMsg);

    private:
        // Device type detection
        bool isHobbywingEscMessage(uint16_t dataTypeId);

        // CAN ID parsing methods
        uint16_t getDataTypeIdFromCanId(uint32_t canId);
        uint8_t getNodeIdFromCanId(uint32_t canId);
};

#endif
