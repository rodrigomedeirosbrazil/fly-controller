#pragma once
#include <stdint.h>
#include "../Telemetry/SignalState.h"

// Pure Hobbywing DroneCAN status-frame decoding: no Arduino deps, host-tested
// in test/HobbywingCanParserTest.cpp.
//
// The ESC broadcasts two single-frame status transfers. Each frame passed in
// here includes the DroneCAN tail byte as its last byte.
//
// StatusMsg2 also carries a pack voltage; it is deliberately NOT decoded.
// Battery voltage on this build comes from the voltage divider only.
namespace HobbywingCanParser {

const uint16_t STATUS_MSG1_DATA_TYPE_ID = 0x4E52;
const uint16_t STATUS_MSG2_DATA_TYPE_ID = 0x4E53;

// Frame layout: payload bytes followed by one tail byte.
const uint8_t STATUS_MSG1_FRAME_LENGTH = 7;
const uint8_t STATUS_MSG1_RPM_LOW_INDEX = 0;
const uint8_t STATUS_MSG1_RPM_HIGH_INDEX = 1;
const uint8_t STATUS_MSG1_DIRECTION_INDEX = 5;
const uint8_t STATUS_MSG1_DIRECTION_CCW_MASK = 0x80;

const uint8_t STATUS_MSG2_FRAME_LENGTH = 6;
const uint8_t STATUS_MSG2_CURRENT_LOW_INDEX = 2;
const uint8_t STATUS_MSG2_CURRENT_HIGH_INDEX = 3;
const uint8_t STATUS_MSG2_TEMPERATURE_INDEX = 4;

// Tail byte: a single-frame transfer has both start and end of transfer set.
const uint8_t TAIL_START_OF_TRANSFER = 0x80;
const uint8_t TAIL_END_OF_TRANSFER = 0x40;

// Current is reported in deciamps (0.1 A).
const uint32_t MILLI_AMPS_PER_DECI_AMP = 100;

struct StatusMsg1 {
    uint16_t rpm;
    bool isCcw;
};

struct StatusMsg2 {
    uint32_t batteryCurrentMilliAmps;
    uint8_t escTemperatureCelsius;
};

// True only for a complete single-frame transfer of the exact expected length.
inline bool isSingleFrameOfLength(const uint8_t* data, uint8_t length, uint8_t expectedLength) {
    if (data == nullptr || length != expectedLength) return false;
    const uint8_t requiredTail = TAIL_START_OF_TRANSFER | TAIL_END_OF_TRANSFER;
    return (data[length - 1] & requiredTail) == requiredTail;
}

// On rejection `out` is left untouched.
inline bool parseStatusMsg1(const uint8_t* data, uint8_t length, StatusMsg1& out) {
    if (!isSingleFrameOfLength(data, length, STATUS_MSG1_FRAME_LENGTH)) return false;
    out.rpm = (uint16_t)(data[STATUS_MSG1_RPM_LOW_INDEX] |
                         (data[STATUS_MSG1_RPM_HIGH_INDEX] << 8));
    out.isCcw = (data[STATUS_MSG1_DIRECTION_INDEX] & STATUS_MSG1_DIRECTION_CCW_MASK) != 0;
    return true;
}

// On rejection `out` is left untouched.
inline bool parseStatusMsg2(const uint8_t* data, uint8_t length, StatusMsg2& out) {
    if (!isSingleFrameOfLength(data, length, STATUS_MSG2_FRAME_LENGTH)) return false;
    // The current word is signed. Read as unsigned, a regenerative (negative)
    // reading would wrap to thousands of amps; a valid positive reading above
    // 3276.7 A is physically impossible, so clamping <= 0 to zero loses nothing.
    const int16_t deciAmps = (int16_t)(data[STATUS_MSG2_CURRENT_LOW_INDEX] |
                                       (data[STATUS_MSG2_CURRENT_HIGH_INDEX] << 8));
    out.batteryCurrentMilliAmps =
        deciAmps <= 0 ? 0 : (uint32_t)deciAmps * MILLI_AMPS_PER_DECI_AMP;
    out.escTemperatureCelsius = data[STATUS_MSG2_TEMPERATURE_INDEX];
    return true;
}

inline SignalState classifyEscTemp(bool fresh, uint8_t celsius, uint8_t maxValidCelsius) {
    if (!fresh) return SignalState::Stale;
    if (celsius > maxValidCelsius) return SignalState::Invalid;
    return SignalState::Valid;
}

}  // namespace HobbywingCanParser
