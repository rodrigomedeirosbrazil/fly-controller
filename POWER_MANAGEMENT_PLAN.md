# Power Management and Thermal Control Plan

## Overview

This document describes the comprehensive power management strategy for the fly-controller system. The goal is to protect all components from overheating and over-current conditions by dynamically limiting power based on multiple sensor readings.

## Current Implementation Status

### ✅ Already Implemented
1. **Battery Voltage Limiting** (`calcBatteryLimit()`)
   - Monitors voltage from ESC
   - Reduces power when voltage drops below minimum
   - Handles voltage sag during high current draw

2. **Motor Temperature Limiting** (`calcMotorTempLimit()`)
   - Monitors NTC sensor on motor
   - Starts reduction at 50°C
   - Linear reduction from 50°C to 60°C

## Available Data Sources

### ESC (Hobbywing via CAN)
- ✅ ESC Temperature (`hobbywing.getTemperature()`)
- ✅ Battery Voltage (`hobbywing.getDeciVoltage()`)
- ✅ Current Consumption (`hobbywing.getDeciCurrent()`)
- ✅ Motor RPM (`hobbywing.getRpm()`)

### BMS (JK BMS via CAN)
- ✅ Battery Temperature Sensor 1 (`jkbms.getTemperature1()`)
- ✅ Battery Temperature Sensor 2 (`jkbms.getTemperature2()`)
- ✅ Total Battery Voltage (`jkbms.getTotalVoltage()`)
- ✅ Battery Current (`jkbms.getCurrent()`)
- ✅ State of Charge - SOC (`jkbms.getSoc()`)
- ✅ Individual Cell Voltages (`jkbms.getMinCellVoltage()`, `jkbms.getMaxCellVoltage()`)
- ✅ Protection Flags (`jkbms.isProtectionActive()`, `jkbms.getProtectionFlags()`)

### Direct Sensors
- ✅ Motor Temperature via NTC (`motorTemp.getTemperature()`)

---

## Proposed Control Strategies

### 1. ESC Temperature Control ⚡

**Status:** Not implemented

**Implementation:**
- Method: `calcEscTempLimit()`
- Data source: `hobbywing.getTemperature()`

**Configuration:**
```cpp
#define ESC_TEMP_REDUCTION_START 80  // Start reducing at 80°C
#define ESC_TEMP_MIN_VALID 0         // Minimum valid reading
#define ESC_TEMP_MAX_VALID 120       // Maximum valid reading
```

**Behavior:**
- Temperature < 80°C: 100% power
- Temperature 80-110°C: Linear reduction 100% → 0%
- Temperature > 110°C: 0% power
- Invalid sensor data: 100% power (fail-safe to other protections)

**Justification:** ESCs can work at high temperatures but components start degrading above 80°C. At 110°C should be at minimum power.

---

### 2. ESC Current Control 🔋

**Status:** Not implemented

**Implementation:**
- Method: `calcEscCurrentLimit()`
- Data source: `hobbywing.getDeciCurrent()` (in deciamperes)
- Technique: Moving average over 5 seconds to avoid cutting on momentary peaks

**Behavior:**
- 0-80A: 100% power
- 80-200A: Linear reduction 100% → 20%
- >200A: 0% power (emergency)

**Justification:** Continuous currents above 80A damage the ESC. Allow short peaks up to 200A.

---

### 3. Battery Current Control (BMS) 🔌

**Status:** Not implemented

**Implementation:**
- Method: `calcBatteryCurrentLimit()`
- Data source: `jkbms.getCurrent()`
- Technique: Moving average over 3 seconds

**Behavior:**
- 0-105A: 100% power
- 105-140A: Linear reduction 100% → 30%
- >140A: 10% power (safe mode)

**Justification:** Protect battery cells from excessive discharge that reduces lifespan.

---

### 4. Battery Temperature Control (BMS) 🌡️

**Status:** Not implemented

**Implementation:**
- Method: `calcBatteryTempLimit()`
- Data source: `max(jkbms.getTemperature1(), jkbms.getTemperature2())`
- Use the highest temperature between both sensors

**Configuration:**
```cpp
#define BATTERY_TEMP_REDUCTION_START 40
#define BATTERY_MAX_TEMP 60
#define BATTERY_TEMP_MIN_VALID -20
#define BATTERY_TEMP_MAX_VALID 80
```

**Behavior:**
- Temperature < 40°C: 100% power
- Temperature 40-60°C: Linear reduction 100% → 0%
- Temperature > 60°C: 0% power
- Invalid sensor data: 100% power (fail-safe to other protections)

**Justification:** Lithium batteries start degrading above 40°C. Above 60°C risk of thermal runaway.

---

### 5. Cell Balance Control (BMS) ⚖️

**Status:** Not implemented

**Implementation:**
- Method: `calcCellBalanceLimit()`
- Data source: `jkbms.getMinCellVoltage()`, `jkbms.getMaxCellVoltage()`
- Calculation: `delta = maxCellVoltage - minCellVoltage`

**Configuration:**
```cpp
#define CELL_BALANCE_OK 0.05       // 50mV - balanced
#define CELL_BALANCE_WARNING 0.15  // 150mV - unbalanced
```

**Behavior:**
- delta < 0.05V: 100% power (balanced battery)
- 0.05V - 0.15V: Linear reduction 100% → 50%
- delta > 0.15V: 30% power (very unbalanced battery)

**Justification:** Imbalance indicates weak cells. Reducing power prevents over-discharging the weakest cell.

---

### 6. State of Charge (SOC) Control (BMS) 🔋📊

**Status:** Not implemented

**Implementation:**
- Method: `calcSocLimit()`
- Data source: `jkbms.getSoc()`
- Technique: Use hysteresis - when SOC rises, only restore power when reaching +5% above limit

**Configuration:**
```cpp
#define SOC_NORMAL 20    // Normal operation
#define SOC_WARNING 10   // Warning level
#define SOC_CRITICAL 5   // Critical level
```

**Behavior:**
- SOC > 20%: 100% power
- 10-20%: Linear reduction 100% → 40%
- 5-10%: 30% power ("return to home" mode)
- < 5%: 10% power (emergency mode)

**Justification:** Avoid deep discharge that permanently damages lithium cells.

---

### 7. BMS Protection Flags 🚨

**Status:** Not implemented

**Implementation:**
- Method: `calcBmsProtectionLimit()`
- Data source: `jkbms.isProtectionActive()`, `jkbms.getProtectionFlags()`

**Behavior:**
- If protection active: 0% power (immediate disarm)
- Emit continuous error beep
- Display protection flags on serial output

**Justification:** BMS has internal protections (overcurrent, overvoltage, short-circuit). When active, system must stop immediately.

---

### 8. Motor RPM Monitoring 🔄

**Status:** Not implemented (future enhancement)

**Implementation:**
- Method: `calcRpmLimit()`
- Data source: `hobbywing.getRpm()`

**Behavior:**
- If RPM = 0 for more than 2 seconds with throttle > 20%: reduce to 0%
- Emit warning beep

**Justification:** Stalled motor draws maximum current without rotation = rapid overheating.

---

### 9. Data Freshness/Timeout ⏱️

**Status:** Partially implemented (`hobbywing.isReady()`)

**Implementation:**
- Add BMS timeout check: `jkbms.isDataFresh()`

**Behavior:**
- If BMS data is stale (>5s): Use only ESC limits
- If ESC data is stale: Use only BMS limits
- If both stale: 50% maximum power (degraded mode)

**Justification:** CAN communication loss should not cause total failure, but should be conservative.

---

## Implementation in Power Class

### Main Power Calculation Method

```cpp
unsigned int Power::calcPower() {
    unsigned int batteryVoltageLimit = calcBatteryLimit();          // ✅ Already implemented
    unsigned int motorTempLimit = calcMotorTempLimit();             // ✅ Already implemented

    // NEW METHODS TO IMPLEMENT:
    unsigned int escTempLimit = calcEscTempLimit();                 // 1. NEW
    unsigned int escCurrentLimit = calcEscCurrentLimit();           // 2. NEW
    unsigned int batteryCurrentLimit = calcBatteryCurrentLimit();   // 3. NEW
    unsigned int batteryTempLimit = calcBatteryTempLimit();         // 4. NEW
    unsigned int cellBalanceLimit = calcCellBalanceLimit();         // 5. NEW
    unsigned int socLimit = calcSocLimit();                         // 6. NEW
    unsigned int bmsProtectionLimit = calcBmsProtectionLimit();     // 7. NEW

    // Return MINIMUM of all limits (most restrictive wins)
    return min({
        batteryVoltageLimit,
        motorTempLimit,
        escTempLimit,
        escCurrentLimit,
        batteryCurrentLimit,
        batteryTempLimit,
        cellBalanceLimit,
        socLimit,
        bmsProtectionLimit
    });
}
```

### Private Members to Add

```cpp
private:
    // Moving average buffers for current limiting
    unsigned int escCurrentHistory[5];
    unsigned int batteryCurrentHistory[3];
    unsigned int escCurrentIndex;
    unsigned int batteryCurrentIndex;

    // SOC hysteresis tracking
    unsigned int lastSocLimit;
    bool socLimitActive;
```

---

## Constants to Add to config.h

```cpp
// ESC Temperature Limits
#define ESC_TEMP_REDUCTION_START 80
#define ESC_TEMP_MIN_VALID 0
#define ESC_TEMP_MAX_VALID 120

// Battery Temperature Limits
#define BATTERY_TEMP_REDUCTION_START 40
#define BATTERY_MAX_TEMP 60
#define BATTERY_TEMP_MIN_VALID -20
#define BATTERY_TEMP_MAX_VALID 80

// Cell Balance Limits
#define CELL_BALANCE_OK 0.05      // 50mV
#define CELL_BALANCE_WARNING 0.15  // 150mV

// SOC Limits
#define SOC_NORMAL 20
#define SOC_WARNING 10
#define SOC_CRITICAL 5

// Hysteresis
#define SOC_HYSTERESIS 5  // Percentage
```

---

## Implementation Priority

### High Priority (Critical Protection)
1. **BMS Protection Flags** (#7)
   - Critical safety feature
   - Prevents damage from BMS-detected faults

2. **Battery Temperature** (#4)
   - Prevents thermal runaway
   - Most dangerous failure mode

3. **ESC Temperature** (#1)
   - Prevents ESC damage
   - Expensive component to replace

### Medium Priority (Important Protection)
4. **Battery Current** (#3)
   - Extends battery lifespan
   - Prevents cell damage

5. **ESC Current** (#2)
   - Prevents ESC damage
   - Complements ESC temperature protection

6. **SOC Limiting** (#6)
   - Prevents deep discharge
   - Critical for battery health

### Low Priority (Optimization)
7. **Cell Balance** (#5)
   - Early warning system
   - Gradual degradation indicator

8. **Data Timeout** (#9)
   - Robustness improvement
   - Handles communication failures

9. **RPM Monitoring** (#8)
   - Future enhancement
   - Stall detection

---

## Design Principles

1. **Fail-Safe**: Most restrictive limit always wins
2. **Gradual Reduction**: Linear power reduction prevents sudden behavior changes
3. **Multi-Layer Protection**: Multiple sensors provide redundancy
4. **Conservative Defaults**: Invalid sensor data defaults to safe values
5. **Hysteresis**: Prevents rapid oscillation at threshold boundaries
6. **Moving Averages**: Smooths current readings to avoid false triggers

---

## Testing Strategy

### Unit Tests
- Test each limit function independently
- Verify boundary conditions
- Test invalid sensor data handling

### Integration Tests
- Test multiple simultaneous limits
- Verify minimum selection logic
- Test moving average calculations

### Field Tests
1. Temperature limits: Controlled heating test
2. Current limits: Progressive load test
3. SOC limits: Full discharge cycle monitoring
4. BMS protection: Trigger protection flags intentionally

---

## Future Enhancements

1. **Predictive Limiting**: Use rate of change to anticipate problems
2. **Adaptive Thresholds**: Learn optimal limits based on usage patterns
3. **Telemetry Logging**: Record which limits are most frequently active
4. **User Configuration**: Allow adjustment of limits via CAN commands
5. **Power Reserve Mode**: Keep 10% reserve for emergency maneuvers

---

## References

- `src/Power/Power.cpp` - Current implementation
- `src/config.h` - System constants
- `src/Hobbywing/Hobbywing.h` - ESC data interface
- `src/Jkbms/Jkbms.h` - BMS data interface

---

**Document Version:** 1.0
**Date:** October 1, 2025
**Author:** Planning document based on system analysis

