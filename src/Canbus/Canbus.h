#ifndef Canbus_h
#define Canbus_h

#include <mcp2515.h>

class Canbus
{
    public:
        static const uint8_t setLedColorRed = 0x04;
        static const uint8_t setLedColorGreen = 0x02;
        static const uint8_t setLedColorBlue = 0x01;

        Canbus(MCP2515 *mcp2515);
        void parseCanMsg(struct can_frame *canMsg);
        bool isReady();
        void setLedColor(uint8_t color);
        uint16_t getRpm() { return rpm; }
        uint16_t getMiliVoltage() { return milliVoltage; }
        uint16_t getMiliCurrent() { return milliCurrent; }
        uint8_t getTemperature() { return temperature; }


    private:
        MCP2515 *mcp2515;

        const uint8_t escNodeId = 0x03;

        const uint16_t statusMsg1 = 0x4E52;
        const uint16_t statusMsg2 = 0x4E53;
        const uint16_t statusMsg3 = 0x4E54;

        const uint8_t setLedDataTypeId = 0xd4;
        const uint8_t setLedOptionSave = 0x01;
        const uint8_t setLedBlinkOff = 0x00;
        const uint8_t setLedBlink1Hz = 0x01;
        const uint8_t setLedBlink2Hz = 0x02;
        const uint8_t setLedBlink5Hz = 0x05;

        unsigned long lastReadStatusMsg1;
        unsigned long lastReadStatusMsg2;

        uint8_t transferId;

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