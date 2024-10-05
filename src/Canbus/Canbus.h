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
};

#endif