#ifndef Canbus_h
#define Canbus_h

#include <mcp2515.h>

#define STATUS_MSG1 0x4E52
#define STATUS_MSG2 0x4E53
#define STATUS_MSG3 0x4E54
class Canbus
{
    public:
        Canbus();
        void parseCanMsg(struct can_frame *canMsg);
        bool isReady();
        uint16_t getRpm() { return rpm; }
        uint16_t getMiliVoltage() { return milliVoltage; }
        uint16_t getMiliCurrent() { return milliCurrent; }
        uint8_t getTemperature() { return temperature; }


    private:
        unsigned long lastReadStatusMsg1;
        unsigned long lastReadStatusMsg2;

        uint8_t temperature;
        uint16_t milliCurrent;
        uint16_t milliVoltage;
        uint16_t rpm;

        void handleStatusMsg1(struct can_frame *canMsg);
        void handleStatusMsg2(struct can_frame *canMsg);

        uint8_t getPriorityFromCanId(uint32_t canId);
        uint16_t getDataTypeIdFromCanId(uint32_t canId);
        uint8_t getNodeIdFromCanId(uint32_t canId);
        bool isServiceFrame(uint32_t canId);
        uint8_t getTailByteFromPayload(uint8_t *payload, uint8_t canDlc);
        bool isStartOfFrame(uint8_t tailByte);
        bool isEndOfFrame(uint8_t tailByte);
        bool isToggleFrame(uint8_t tailByte);
        uint8_t getTransferId(uint8_t tailByte);
        uint8_t getTemperatureFromPayload(uint8_t *payload);
        uint16_t getMiliCurrentFromPayload(uint8_t *payload);
        uint16_t getMiliVoltageFromPayload(uint8_t *payload);
        uint16_t getRpmFromPayload(uint8_t *payload);
};

#endif