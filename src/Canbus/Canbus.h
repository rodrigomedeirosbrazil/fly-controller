#ifndef Canbus_h
#define Canbus_h

#include <mcp2515.h>

MCP2515 mcp2515(10);
struct can_frame canMsg;

class Canbus
{
    public:
        Canbus();
        void tick();

    private:
        struct PACKED {
            uint32_t counter;
            uint16_t throttle_req;
            uint16_t throttle;
            uint16_t rpm;
            uint16_t voltage;
            int16_t current;
            int16_t phase_current;
            uint8_t mos_temperature;
            uint8_t cap_temperature;
            uint16_t status;
            uint16_t crc;
        } pkt;

        bool parsePacket(void);


};

#endif