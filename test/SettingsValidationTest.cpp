#include <cassert>
#include <cstdint>
#include <iostream>
#include "../src/Settings/SettingsValidation.h"
using namespace std;

void test_power_capacity_range() {
    assert(validatePower(1000,  48000, 58800) == SettingsError::None);
    assert(validatePower(65000, 48000, 58800) == SettingsError::None);
    assert(validatePower(999,   48000, 58800) == SettingsError::CapacityRange);
    assert(validatePower(65001, 48000, 58800) == SettingsError::CapacityRange);
}

void test_power_voltage_ranges() {
    assert(validatePower(20000, 2500,  63000) == SettingsError::None);
    assert(validatePower(20000, 2499,  58800) == SettingsError::MinVoltageRange);
    assert(validatePower(20000, 63001, 58800) == SettingsError::MinVoltageRange);
    assert(validatePower(20000, 48000, 2499)  == SettingsError::MaxVoltageRange);
    assert(validatePower(20000, 48000, 63001) == SettingsError::MaxVoltageRange);
}

void test_power_does_not_check_voltage_ordering() {
    // min < max has never been enforced. Adding the rule is a separate
    // decision about existing behaviour.
    assert(validatePower(20000, 58800, 48000) == SettingsError::None);
}

void test_voltage_divider_ratio_range() {
    assert(validateVoltageDividerRatio(1.0f)    == SettingsError::None);
    assert(validateVoltageDividerRatio(100.0f)  == SettingsError::None);
    assert(validateVoltageDividerRatio(0.99f)   == SettingsError::DividerRatioRange);
    assert(validateVoltageDividerRatio(100.01f) == SettingsError::DividerRatioRange);
}

void test_thermal_ranges() {
    assert(validateThermal(80000, 100000, 80000, 110000) == SettingsError::None);
    assert(validateThermal(0,     150000, 0,     150000) == SettingsError::None);
    assert(validateThermal(80000, 150001, 80000, 110000) == SettingsError::MotorTempRange);
    assert(validateThermal(-1,    100000, 80000, 110000) == SettingsError::MotorTempRange);
    assert(validateThermal(80000, 100000, 80000, 150001) == SettingsError::EscTempRange);
    assert(validateThermal(80000, 100000, -1,    110000) == SettingsError::EscTempRange);
}

void test_thermal_does_not_check_ordering() {
    // Same reasoning as the voltage pair: not a rule enforced today.
    assert(validateThermal(100000, 80000, 110000, 80000) == SettingsError::None);
}

void test_motor_temp_source_range() {
    assert(validateMotorTempSource(0) == SettingsError::None);
    assert(validateMotorTempSource(1) == SettingsError::None);  // Ads1115
    assert(validateMotorTempSource(2) == SettingsError::MotorTempSourceInvalid);
}

void test_bms_type_range() {
    assert(validateBmsType(0) == SettingsError::None);
    assert(validateBmsType(3) == SettingsError::None);  // BmsTypeJk, the highest
    assert(validateBmsType(4) == SettingsError::BmsTypeInvalid);
}

void test_system_ranges() {
    assert(validateSystem(0,   0) == SettingsError::None);
    assert(validateSystem(100, 1) == SettingsError::None);
    assert(validateSystem(101, 0) == SettingsError::BuzzerVolumeRange);
    assert(validateSystem(50,  2) == SettingsError::ThrottleSourceInvalid);
}

void test_reference_current_range() {
    assert(validateReferenceCurrent(10)  == SettingsError::None);
    assert(validateReferenceCurrent(300) == SettingsError::None);
    assert(validateReferenceCurrent(9)   == SettingsError::ReferenceCurrentRange);
    assert(validateReferenceCurrent(301) == SettingsError::ReferenceCurrentRange);
    assert(validateReferenceCurrent(0)   == SettingsError::ReferenceCurrentRange);
}

void test_reference_current_clamp() {
    assert(clampReferenceCurrent(0)    == 10);
    assert(clampReferenceCurrent(90)   == 90);
    assert(clampReferenceCurrent(5000) == 300);
}

void test_capacity_range_alone() {
    // validateCapacity is the range validatePower applies; BmsCapacitySync
    // uses it directly so the two can never disagree.
    assert(validateCapacity(1000)  == SettingsError::None);
    assert(validateCapacity(65000) == SettingsError::None);
    assert(validateCapacity(999)   == SettingsError::CapacityRange);
    assert(validateCapacity(65001) == SettingsError::CapacityRange);
    assert(validateCapacity(0)     == SettingsError::CapacityRange);
}

int main() {
    test_power_capacity_range();
    test_capacity_range_alone();
    test_power_voltage_ranges();
    test_power_does_not_check_voltage_ordering();
    test_voltage_divider_ratio_range();
    test_thermal_ranges();
    test_thermal_does_not_check_ordering();
    test_motor_temp_source_range();
    test_bms_type_range();
    test_system_ranges();
    test_reference_current_range();
    test_reference_current_clamp();
    cout << "SettingsValidationTest: all passed" << endl;
    return 0;
}
