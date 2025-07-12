#ifndef Canbus_h
#define Canbus_h

#include <mcp2515.h>

class Canbus
{
    public:
        static const uint8_t ledColorRed = 0x04;
        static const uint8_t ledColorGreen = 0x02;
        static const uint8_t ledColorBlue = 0x01;

        Canbus();
        void parseCanMsg(struct can_frame *canMsg);
        bool isReady();
        void setLedColor(uint8_t color);
        void setDirection(bool isCcw);
        uint16_t getRpm() { return rpm; }
        uint16_t getDeciVoltage() { return deciVoltage; }
        uint16_t getDeciCurrent() { return deciCurrent; }
        uint8_t getTemperature() { return temperature; }


    private:
        const uint8_t nodeId = 0x7F;
        const uint8_t escNodeId = 0x03;

        const uint16_t statusMsg1 = 0x4E52;
        const uint16_t statusMsg2 = 0x4E53;
        const uint16_t statusMsg3 = 0x4E54;

        unsigned long lastReadStatusMsg1;
        unsigned long lastReadStatusMsg2;

        uint8_t transferId;

        uint8_t temperature;
        uint16_t deciCurrent;
        uint16_t deciVoltage;
        uint16_t rpm;
        bool isCcwDirection;

        void handleStatusMsg1(struct can_frame *canMsg);
        void handleStatusMsg2(struct can_frame *canMsg);

        uint8_t getPriorityFromCanId(uint32_t canId);
        uint16_t getDataTypeIdFromCanId(uint32_t canId);
        uint8_t getServiceTypeIdFromCanId(uint32_t canId);
        uint8_t getNodeIdFromCanId(uint32_t canId);
        uint8_t getDestNodeIdFromCanId(uint32_t canId);
        bool isServiceFrame(uint32_t canId);
        bool isRequestFrame(uint32_t canId);
        uint8_t getTailByteFromPayload(uint8_t *payload, uint8_t canDlc);
        bool isStartOfFrame(uint8_t tailByte);
        bool isEndOfFrame(uint8_t tailByte);
        bool isToggleFrame(uint8_t tailByte);
        uint8_t getTransferId(uint8_t tailByte);
        uint8_t getTemperatureFromPayload(uint8_t *payload);
        uint16_t getDeciCurrentFromPayload(uint8_t *payload);
        uint16_t getDeciVoltageFromPayload(uint8_t *payload);
        uint16_t getRpmFromPayload(uint8_t *payload);
        bool getDirectionCCWFromPayload(uint8_t *payload);

        void sendMessage(
            uint8_t priority,
            uint8_t serviceTypeId,
            uint8_t destNodeId,
            uint8_t *payload,
            uint8_t payloadLength
        );
};

#endif
