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
    static const uint8_t ledColorRed = 0x04;
    static const uint8_t ledColorGreen = 0x02;
    static const uint8_t ledColorBlue = 0x01;
    static const uint8_t ledBlinkOff = 0x00;
    static const uint8_t ledBlink1Hz = 0x01;
    static const uint8_t ledBlink2Hz = 0x02;
    static const uint8_t ledBlink5Hz = 0x05;

    static const uint8_t throttleSourceCAN = 0x00;
    static const uint8_t throttleSourcePWM = 0x01;

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
    uint8_t getTemperatureFromPayload(uint8_t *payload);
    uint32_t getBatteryCurrentFromPayload(uint8_t *payload);
    uint16_t getBatteryVoltageMilliVoltsFromPayload(uint8_t *payload);
    uint16_t getRpmFromPayload(uint8_t *payload);
    bool getDirectionCCWFromPayload(uint8_t *payload);
    void sendMessage(uint8_t priority, uint16_t serviceTypeId, uint8_t destNodeId,
                     uint8_t *payload, uint8_t payloadLength);
    void handleGetEscIdResponse(twai_message_t *canMsg);
    uint8_t getEscThrottleIdFromPayload(uint8_t *payload);
};

#endif
