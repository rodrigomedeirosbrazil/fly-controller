#ifndef Hobbywing_h
#define Hobbywing_h

#include <mcp2515.h>

class Hobbywing
{
public:
    // ESC control constants
    static const uint8_t ledColorRed = 0x04;
    static const uint8_t ledColorGreen = 0x02;
    static const uint8_t ledColorBlue = 0x01;
    static const uint8_t ledBlinkOff = 0x00;
    static const uint8_t ledBlink1Hz = 0x01;
    static const uint8_t ledBlink2Hz = 0x02;
    static const uint8_t ledBlink5Hz = 0x05;

    static const uint8_t throttleSourceCAN = 0x00;
    static const uint8_t throttleSourcePWM = 0x01;

    Hobbywing();

    // Main parsing method for ESC messages
    void parseEscMessage(struct can_frame *canMsg);

    // ESC control methods
    void announce();
    void setLedColor(uint8_t color, uint8_t blink = ledBlinkOff);
    void setDirection(bool isCcw);
    void setThrottleSource(uint8_t source);
    void setRawThrottle(int16_t throttle);
    void requestEscId();

    // Getters for ESC data
    uint16_t getRpm() { return rpm; }
    uint16_t getDeciVoltage() { return deciVoltage; }
    uint16_t getDeciCurrent() { return deciCurrent; }
    uint8_t getTemperature() { return temperature; }
    bool getDirectionCCW() { return isCcwDirection; }

    // Connectivity status
    bool isReady();

private:
    // ESC-specific data
    uint8_t temperature;
    uint16_t deciCurrent;
    uint16_t deciVoltage;
    uint16_t rpm;
    bool isCcwDirection;

    // Timestamps for connectivity control
    unsigned long lastReadStatusMsg1;
    unsigned long lastReadStatusMsg2;
    unsigned long lastAnnounce;

    // Transfer control
    uint8_t transferId;

    // CAN bus constants
    const uint8_t nodeId = 0x13;
    const uint8_t escNodeId = 0x03;
    const uint8_t escThrottleId = 0x03;

    // Hobbywing protocol constants
    const uint16_t statusMsg1 = 0x4E52;
    const uint16_t statusMsg2 = 0x4E53;
    const uint16_t statusMsg3 = 0x4E54;
    const uint16_t getEscIdRequestDataTypeId = 0x4E56;

    // Hobbywing-specific parsing methods
    void handleStatusMsg1(struct can_frame *canMsg);
    void handleStatusMsg2(struct can_frame *canMsg);

    // Hobbywing payload data extraction methods
    uint8_t getTailByteFromPayload(uint8_t *payload, uint8_t canDlc);
    bool isStartOfFrame(uint8_t tailByte);
    bool isEndOfFrame(uint8_t tailByte);
    bool isToggleFrame(uint8_t tailByte);
    uint8_t getTemperatureFromPayload(uint8_t *payload);
    uint16_t getDeciCurrentFromPayload(uint8_t *payload);
    uint16_t getDeciVoltageFromPayload(uint8_t *payload);
    uint16_t getRpmFromPayload(uint8_t *payload);
    bool getDirectionCCWFromPayload(uint8_t *payload);

    // ESC control helper methods
    void sendMessage(
        uint8_t priority,
        uint16_t serviceTypeId,
        uint8_t destNodeId,
        uint8_t *payload,
        uint8_t payloadLength
    );
    void handleGetEscIdResponse(struct can_frame *canMsg);
    uint8_t getTailByteFromPayload(uint8_t *payload, uint8_t canDlc);
    uint8_t getTransferId(uint8_t tailByte);
    uint8_t getEscThrottleIdFromPayload(uint8_t *payload);

    // CAN ID parsing methods
    uint8_t getPriorityFromCanId(uint32_t canId);
    uint16_t getDataTypeIdFromCanId(uint32_t canId);
    uint8_t getServiceTypeIdFromCanId(uint32_t canId);
    uint8_t getNodeIdFromCanId(uint32_t canId);
    uint8_t getDestNodeIdFromCanId(uint32_t canId);
    bool isServiceFrame(uint32_t canId);
    bool isRequestFrame(uint32_t canId);
};

#endif
