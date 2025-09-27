#ifndef Canbus_h
#define Canbus_h

#include <mcp2515.h>
#include "../Hobbywing/Hobbywing.h"

class Canbus
{
    public:

        Canbus();
        void parseCanMsg(struct can_frame *canMsg);
        void printCanMsg(struct can_frame *canMsg);

        // ESC control methods - delegate to Hobbywing
        void setLedColor(uint8_t color, uint8_t blink = Hobbywing::ledBlinkOff) {
            hobbywing.setLedColor(color, blink);
        }
        void setDirection(bool isCcw) {
            hobbywing.setDirection(isCcw);
        }
        void setThrottleSource(uint8_t source) {
            hobbywing.setThrottleSource(source);
        }
        void setRawThrottle(int16_t throttle) {
            hobbywing.setRawThrottle(throttle);
        }
        void requestEscId() {
            hobbywing.requestEscId();
        }

        // ESC data getters - delegate to Hobbywing
        uint16_t getRpm() { return hobbywing.getRpm(); }
        uint16_t getDeciVoltage() { return hobbywing.getDeciVoltage(); }
        uint16_t getDeciCurrent() { return hobbywing.getDeciCurrent(); }
        uint8_t getTemperature() { return hobbywing.getTemperature(); }

    private:
        // Hobbywing ESC instance
        Hobbywing hobbywing;

        const uint8_t nodeId = 0x13;
        const uint8_t escNodeId = 0x03;
        const uint8_t escThrottleId = 0x03;

        const uint16_t getEscIdRequestDataTypeId = 0x4E56; // 20054 = 0x4E56

        unsigned long lastAnnounce;


        uint8_t transferId;

        // Device type detection
        bool isHobbywingEscMessage(uint16_t dataTypeId);

        uint8_t getPriorityFromCanId(uint32_t canId);
        uint16_t getDataTypeIdFromCanId(uint32_t canId);
        uint8_t getServiceTypeIdFromCanId(uint32_t canId);
        uint8_t getNodeIdFromCanId(uint32_t canId);
        uint8_t getDestNodeIdFromCanId(uint32_t canId);
        bool isServiceFrame(uint32_t canId);
        bool isRequestFrame(uint32_t canId);
        uint8_t getTransferId(uint8_t tailByte);
        uint8_t getTailByteFromPayload(uint8_t *payload, uint8_t canDlc);
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
