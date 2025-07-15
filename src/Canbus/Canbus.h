#ifndef Canbus_h
#define Canbus_h

#include <mcp2515.h>

class Canbus
{
    public:
        static const uint8_t ledColorRed = 0x04;
        static const uint8_t ledColorGreen = 0x02;
        static const uint8_t ledColorBlue = 0x01;
        static const uint8_t throttleSourceCAN = 0x00;
        static const uint8_t throttleSourcePWM = 0x01;

        Canbus();
        void announce();
        void parseCanMsg(struct can_frame *canMsg);
        void printCanMsg(struct can_frame *canMsg);
        bool isReady();
        void setLedColor(uint8_t color);
        void setDirection(bool isCcw);
        void setThrottleSource(uint8_t source);
        void setRawThrottle(int16_t throttle);
        uint16_t getRpm() { return rpm; }
        uint16_t getDeciVoltage() { return deciVoltage; }
        uint16_t getDeciCurrent() { return deciCurrent; }
        uint8_t getTemperature() { return temperature; }
        void requestEscId();

    private:
        const uint8_t nodeId = 0x13;
        const uint8_t escNodeId = 0x03;
        const uint8_t escThrottleId = 0x03;

        const uint16_t statusMsg1 = 0x4E52;
        const uint16_t statusMsg2 = 0x4E53;
        const uint16_t statusMsg3 = 0x4E54;
        const uint16_t getEscIdRequestDataTypeId = 0x4E56; // 20054 = 0x4E56

        unsigned long lastReadStatusMsg1;
        unsigned long lastReadStatusMsg2;
        unsigned long lastAnnounce;


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
        uint8_t getEscThrottleIdFromPayload(uint8_t *payload);
        void handleGetEscIdResponse(struct can_frame *canMsg);


        void sendMessage(
            uint8_t priority,
            uint16_t serviceTypeId,
            uint8_t destNodeId,
            uint8_t *payload,
            uint8_t payloadLength
        );
};

#endif
