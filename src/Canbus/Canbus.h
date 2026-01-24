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

    private:
        // Device type detection
        bool isHobbywingEscMessage(uint16_t dataTypeId);
        bool isTmotorEscMessage(uint16_t dataTypeId);
};

#endif
