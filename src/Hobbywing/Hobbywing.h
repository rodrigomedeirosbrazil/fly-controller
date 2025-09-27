#ifndef Hobbywing_h
#define Hobbywing_h

#include <mcp2515.h>

class Hobbywing
{
public:
    Hobbywing();

    // Main parsing method for ESC messages
    void parseEscMessage(struct can_frame *canMsg);

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

    // Hobbywing protocol constants
    const uint16_t statusMsg1 = 0x4E52;
    const uint16_t statusMsg2 = 0x4E53;
    const uint16_t statusMsg3 = 0x4E54;

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
};

#endif
