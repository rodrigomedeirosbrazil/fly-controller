#pragma once

#include <stdint.h>

// Offset and minimum length of the RPM field (int18, 3 bytes, little-endian)
// in a reassembled ESC_STATUS payload.
static const uint8_t ESC_STATUS_RPM_FIELD_OFFSET = 12;
static const uint8_t ESC_STATUS_RPM_MIN_LEN = 15;
static const uint32_t ESC_STATUS_RPM_MASK = 0x3FFFF;       // 18 bits
static const uint32_t ESC_STATUS_RPM_SIGN_BIT = 0x20000;   // bit 17
static const uint32_t ESC_STATUS_RPM_SIGN_EXTEND = 0xFFFC0000;

// Parses the RPM from an ESC_STATUS payload. Returns false and leaves
// rpmOut untouched when the transfer is too short to hold the field, so a
// short transfer is never mistaken for a real zero RPM.
inline bool parseEscStatusRpm(const uint8_t* payload, uint8_t len, uint16_t* rpmOut) {
    if (len < ESC_STATUS_RPM_MIN_LEN) return false;
    uint32_t raw = (uint32_t)payload[ESC_STATUS_RPM_FIELD_OFFSET]
                 | ((uint32_t)payload[ESC_STATUS_RPM_FIELD_OFFSET + 1] << 8)
                 | ((uint32_t)payload[ESC_STATUS_RPM_FIELD_OFFSET + 2] << 16);
    raw &= ESC_STATUS_RPM_MASK;
    int32_t value = (int32_t)raw;
    if (raw & ESC_STATUS_RPM_SIGN_BIT) value = (int32_t)(raw | ESC_STATUS_RPM_SIGN_EXTEND);
    if (value < 0) value = -value;
    *rpmOut = (uint16_t)value;
    return true;
}
