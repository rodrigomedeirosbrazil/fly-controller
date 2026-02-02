#ifndef Hobbywing_h
#define Hobbywing_h

#include <driver/twai.h>

/**
 * Hobbywing ESC Communication Class
 *
 * This class implements DroneCAN communication protocol for Hobbywing Electronic Speed Controllers (ESC).
 * Specifically designed to work with the Hobbywing X13 motor ESC.
 *
 * Developed by: Rodrigo Medeiros
 *
 * This implementation is based on:
 * - ArduPilot codebase (https://github.com/ArduPilot/ardupilot)
 * - Various GitHub repositories implementing DroneCAN ESC protocols
 *
 * Key features:
 * - DroneCAN message parsing and generation
 * - ESC telemetry reading (RPM, voltage, current, temperature)
 * - ESC control (LED color, direction, throttle source)
 * - Raw throttle command transmission
 * - ESC ID discovery and communication setup
 *
 * For developers searching for Hobbywing ESC integration:
 * - Compatible with Hobbywing X13 ESC
 * - Uses DroneCAN protocol over CAN bus
 * - Supports MCP2515 CAN controller
 * - Implements standard DroneCAN message formats
 * - Provides both telemetry and control capabilities
 */
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
    void parseEscMessage(twai_message_t *canMsg);

    // ESC control methods
    void announce();
    void setLedColor(uint8_t color, uint8_t blink = ledBlinkOff);
    void setDirection(bool isCcw);
    void setThrottleSource(uint8_t source);
    void setRawThrottle(int16_t throttle);
    void requestEscId();
    void handle();

    // Getters for ESC data
    uint16_t getRpm() { return rpm; }
    uint16_t getBatteryVoltageMilliVolts() { return batteryVoltageMilliVolts; }
    uint8_t getBatteryCurrent() { return batteryCurrent; }
    uint8_t getTemperature() { return temperature; }
    bool getDirectionCCW() { return isCcwDirection; }

    // Connectivity status
    bool isReady();

private:
    // ESC-specific data
    uint8_t temperature;
    uint8_t batteryCurrent;
    uint16_t batteryVoltageMilliVolts;
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
    void handleStatusMsg1(twai_message_t *canMsg);
    void handleStatusMsg2(twai_message_t *canMsg);

    // Hobbywing payload data extraction methods
    uint8_t getTemperatureFromPayload(uint8_t *payload);
    uint8_t getBatteryCurrentFromPayload(uint8_t *payload);
    uint16_t getBatteryVoltageMilliVoltsFromPayload(uint8_t *payload);
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
    void handleGetEscIdResponse(twai_message_t *canMsg);
    uint8_t getEscThrottleIdFromPayload(uint8_t *payload);
};

#endif
