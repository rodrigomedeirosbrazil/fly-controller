#ifndef HobbywingCan_h
#define HobbywingCan_h

#include <stdint.h>
#include <driver/twai.h>

/**
 * Hobbywing ESC DroneCAN telemetry reader.
 *
 * Telemetry only: the throttle is PWM and the controller transmits nothing to
 * the ESC. Frame decoding lives in the pure, host-tested HobbywingCanParser.
 *
 * Two status transfers arrive from the ESC: StatusMsg1 (RPM, direction) and
 * StatusMsg2 (battery current, ESC temperature). Each is considered fresh for
 * TELEMETRY_FRESH_MS after the last frame the parser accepted.
 */
class HobbywingCan {
public:
    static const unsigned long TELEMETRY_FRESH_MS = 1000;

    void parseEscMessage(twai_message_t* canMsg);

    // StatusMsg2 (current, ESC temperature) received within TELEMETRY_FRESH_MS.
    bool hasTelemetry() const;
    // StatusMsg1 (RPM, direction) received within TELEMETRY_FRESH_MS.
    bool hasRpm() const;

    uint16_t getRpm() const { return rpm; }
    uint32_t getBatteryCurrentMilliAmps() const { return batteryCurrentMilliAmps; }
    uint8_t getEscTemperature() const { return escTemperatureCelsius; }
    bool getDirectionCCW() const { return directionCcw; }

private:
    uint16_t rpm = 0;
    bool directionCcw = false;
    uint32_t batteryCurrentMilliAmps = 0;
    uint8_t escTemperatureCelsius = 0;

    // 0 means "never received".
    unsigned long lastReadStatusMsg1 = 0;
    unsigned long lastReadStatusMsg2 = 0;
};

#endif
