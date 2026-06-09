#ifndef REMOTE_LINK_PROTOCOL_H
#define REMOTE_LINK_PROTOCOL_H

#include <stdint.h>

// Shared ESP-NOW wire contract between the controller and the remote throttle.
// MUST stay byte-identical on both sides — both firmwares include this header.

// Fixed radio channel: ESP-NOW peers and the controller softAP must share a
// channel on the ESP32-C3's single radio. Matches the softAP default.
#define REMOTE_LINK_CHANNEL 1

// Beep commands the controller asks the remote to play. Mapped to named
// Buzzer patterns on the remote side.
namespace RemoteBeep {
    enum Type : uint8_t {
        None = 0,
        SystemStart = 1,
        CalibrationStep = 2,
        Armed = 3,
        Disarmed = 4,
        Alert = 5,
        Stop = 6,
    };
}

#pragma pack(push, 1)

// Remote -> Controller, sent ~50 Hz.
// The remote forwards the RAW button state; the controller runs its existing
// AceButton arming gesture on the received state (identical wired/wireless
// behavior). reserved keeps the struct at a stable 4 bytes for future use.
struct ThrottleToControllerPacket {
    uint16_t hallRaw;       // raw Hall ADC reading
    uint8_t  buttonPressed; // 1 = button physically pressed, 0 = released
    uint8_t  reserved;      // padding / future use
};

// Controller -> Remote, heartbeat ~5 Hz + on-change.
struct ControllerToThrottlePacket {
    uint8_t armed;              // 1 = armed (red LED), 0 = disarmed (green LED)
    uint8_t calibrating;        // 1 = controller is in calibration sweep
    uint8_t beepCommandCounter; // monotonic; +1 per beep request
    uint8_t beepCommand;        // RemoteBeep::Type
};

#pragma pack(pop)

// Counter-based dedup shared by both sides: act on a received event only when
// its monotonic counter differs from the last one seen. Robust against the
// 50 Hz repetition and packet loss without needing ACKs. Updates `lastSeen`.
inline bool remoteLinkCounterAdvanced(uint8_t incoming, uint8_t &lastSeen) {
    if (incoming != lastSeen) {
        lastSeen = incoming;
        return true;
    }
    return false;
}

#endif // REMOTE_LINK_PROTOCOL_H
