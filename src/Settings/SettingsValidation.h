#pragma once
#include <stdint.h>

// Range validation for persisted settings: the single source of the ranges
// BleControl's CFG_SET enforces. Pure: no Arduino, no Settings instance,
// host-tested in test/SettingsValidationTest.cpp.
//
// There is deliberately NO ordering check (min < max, reductionStart <
// maxTemp): none has ever existed, and adding one here would silently change
// what is accepted.

enum class SettingsError : uint8_t {
    None = 0,
    CapacityRange,
    MinVoltageRange,
    MaxVoltageRange,
    DividerRatioRange,
    MotorTempRange,
    EscTempRange,
    MotorTempSourceInvalid,
    BmsTypeInvalid,
    BuzzerVolumeRange,
    ThrottleSourceInvalid,
};

inline SettingsError validatePower(uint32_t capacityMah, uint32_t minMv, uint32_t maxMv) {
    if (capacityMah < 1000 || capacityMah > 65000) return SettingsError::CapacityRange;
    if (minMv < 2500 || minMv > 63000)             return SettingsError::MinVoltageRange;
    if (maxMv < 2500 || maxMv > 63000)             return SettingsError::MaxVoltageRange;
    return SettingsError::None;
}

// Kept separate because the divider ratio is optional on the wire (a short
// write from an older app does not carry it), so callers apply it only when
// the field is present. Takes a float so the boundary is exact.
inline SettingsError validateVoltageDividerRatio(float ratio) {
    if (ratio < 1.0f || ratio > 100.0f) return SettingsError::DividerRatioRange;
    return SettingsError::None;
}

inline SettingsError validateThermal(int32_t motorReductionStartMc, int32_t motorMaxMc,
                                     int32_t escReductionStartMc,   int32_t escMaxMc) {
    if (motorMaxMc < 0 || motorMaxMc > 150000)                       return SettingsError::MotorTempRange;
    if (motorReductionStartMc < 0 || motorReductionStartMc > 150000) return SettingsError::MotorTempRange;
    if (escMaxMc < 0 || escMaxMc > 150000)                           return SettingsError::EscTempRange;
    if (escReductionStartMc < 0 || escReductionStartMc > 150000)     return SettingsError::EscTempRange;
    return SettingsError::None;
}

// Highest valid value is MotorTempSourceAds1115 (1) -- see Settings.h.
inline SettingsError validateMotorTempSource(uint8_t source) {
    if (source > 1) return SettingsError::MotorTempSourceInvalid;
    return SettingsError::None;
}

// Highest valid value is BmsTypeJk (3) -- see Settings.h, which notes that
// BmsTypeJk must remain the highest.
inline SettingsError validateBmsType(uint8_t type) {
    if (type > 3) return SettingsError::BmsTypeInvalid;
    return SettingsError::None;
}

inline SettingsError validateSystem(uint8_t buzzerVolume, uint8_t throttleSource) {
    if (buzzerVolume > 100)  return SettingsError::BuzzerVolumeRange;
    if (throttleSource > 1)  return SettingsError::ThrottleSourceInvalid;
    return SettingsError::None;
}
