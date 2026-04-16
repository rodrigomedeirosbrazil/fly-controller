#ifndef HobbywingCan_h
#define HobbywingCan_h

#include <driver/twai.h>

/**
 * Hobbywing ESC CAN protocol driver (DroneCAN)
 * Parse of frames + send commands. Agnostic of telemetry aggregation.
 */
class HobbywingCan
{
public:
    // LED color constants
    static const uint8_t ledColorRed = 0x04;
    static const uint8_t ledColorGreen = 0x02;
    static const uint8_t ledColorBlue = 0x01;
    static const uint8_t ledBlinkOff = 0x00;
    static const uint8_t ledBlink1Hz = 0x01;
    static const uint8_t ledBlink2Hz = 0x02;
    static const uint8_t ledBlink5Hz = 0x05;

    // Throttle source constants
    static const uint8_t throttleSourceCAN = 0x00;
    static const uint8_t throttleSourcePWM = 0x01;

    // Status message frame validation constants
    static const uint8_t STATUS_MSG1_FRAME_LENGTH = 7;
    static const uint8_t STATUS_MSG2_FRAME_LENGTH = 6;

    // Payload field offsets and constants
    static const uint8_t TEMP_OFFSET = 4;
    static const uint8_t CURRENT_HIGH_BYTE_OFFSET = 3;
    static const uint8_t CURRENT_LOW_BYTE_OFFSET = 2;
    static const uint8_t VOLTAGE_HIGH_BYTE_OFFSET = 1;
    static const uint8_t VOLTAGE_LOW_BYTE_OFFSET = 0;
    static const uint8_t RPM_HIGH_BYTE_OFFSET = 1;
    static const uint8_t RPM_LOW_BYTE_OFFSET = 0;
    static const uint8_t DIRECTION_OFFSET = 5;
    static const uint8_t DIRECTION_MASK = 0x80;
    static const uint8_t THROTTLE_ID_OFFSET = 1;

    HobbywingCan();

    void parseEscMessage(twai_message_t *canMsg);
    void setLedColor(uint8_t color, uint8_t blink = ledBlinkOff);
    void setDirection(bool isCcw);
    void setThrottleSource(uint8_t source);
    void setRawThrottle(int16_t throttle);
    void requestEscId();
    void handle();

    uint16_t getRpm() { return rpm; }
    uint16_t getBatteryVoltageMilliVolts() { return batteryVoltageMilliVolts; }
    uint32_t getBatteryCurrentMilliAmps() { return batteryCurrentMilliAmps; }
    uint8_t getEscTemperature() { return temperature; }
    bool getDirectionCCW() { return isCcwDirection; }

    bool isReady();
    bool hasTelemetry();

private:
    uint8_t temperature;
    uint32_t batteryCurrentMilliAmps;
    uint16_t batteryVoltageMilliVolts;
    uint16_t rpm;
    bool isCcwDirection;

    unsigned long lastReadStatusMsg1;
    unsigned long lastReadStatusMsg2;
    uint8_t transferId;

    const uint8_t escNodeId = 0x03;
    const uint8_t escThrottleId = 0x03;

    const uint16_t statusMsg1 = 0x4E52;
    const uint16_t statusMsg2 = 0x4E53;
    const uint16_t statusMsg3 = 0x4E54;
    const uint16_t getEscIdRequestDataTypeId = 0x4E56;

    void handleStatusMsg1(twai_message_t *canMsg);
    void handleStatusMsg2(twai_message_t *canMsg);
    void sendMessage(uint8_t priority, uint16_t serviceTypeId, uint8_t destNodeId,
                     uint8_t *payload, uint8_t payloadLength);
    void handleGetEscIdResponse(twai_message_t *canMsg);
};

#endif
