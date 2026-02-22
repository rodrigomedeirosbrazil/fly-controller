# Fly Controller - AI Assistant Guide

## Overview

**Fly Controller** is an advanced firmware for an intelligent flight controller system designed for drones and UAVs. Built on the PlatformIO framework for the ESP32-C3 platform, it provides comprehensive power management, battery monitoring, and ESC communication using the DroneCAN protocol, along with Bluetooth LE telemetry.

**Code Standards:**
- All code MUST be written in English, including all comments.
- Follow C++/Arduino best practices.
- Maintain a modular architecture with a clear separation of concerns.
- Document complex algorithms and protocol implementations.

## Key Technologies & Libraries

### Framework & Platform
- **Framework:** Arduino
- **Platform:** Espressif 32 (specifically `lolin_c3_mini`)
- **Build System:** PlatformIO

### Core Libraries
- **`madhephaestus/ESP32Servo`**: PWM signal generation for ESC control on ESP32.
- **`bxparks/AceButton@^1.10.1`**: Advanced button handling with event detection.

### Communication
- **CAN Bus:** On-chip TWAI controller for DroneCAN.
- **Bluetooth LE:** For telemetry transmission via the Xctod module.

## Project Architecture

### Hardware Configuration
- **Microcontroller:** ESP32-C3 Super Mini
- **CAN Transceiver:** SN65HVD230 or similar
- **CAN Bus Speed:** 500 kbps (Hobbywing), 1 Mbps (T-Motor)
- **PWM Output:** ESC control (1050-1950μs)
- **Analog Inputs:**
  - GPIO 1: NTC temperature sensor (motor)
  - GPIO 0: Hall sensor (throttle input)
- **Digital I/O:**
  - GPIO 5: Push button
  - GPIO 6: Passive buzzer (PWM output)
  - GPIO 7: ESC PWM output
  - GPIO 2/3: CAN TX/RX for the TWAI controller

### File Structure

```
src/
├── main.cpp              # Application entry point (setup/loop)
├── main.h                # Function declarations for main
├── config.h              # Global configuration and pin definitions
├── config.cpp            # Global object instantiations
├── config_controller.h   # Build type (IS_HOBBYWING, IS_TMOTOR, IS_XAG, USES_CAN_BUS)
│
├── Button/               # User input handling
├── Buzzer/               # Audible feedback system (PWM)
├── Canbus/               # CAN receive() API, NodeStatus (TWAI)
├── Power/                # Intelligent power management
├── BatteryMonitor/       # Coulomb counting, SoC from voltage
├── Settings/             # Persistent configuration (Preferences)
│
├── Hobbywing/            # DroneCAN (Hobbywing build)
│   ├── HobbywingCan.h/cpp    # ESC protocol
│   └── HobbywingTelemetry.h/cpp  # Telemetry aggregation
│
├── Tmotor/               # UAVCAN (T-Motor build)
│   ├── TmotorCan.h/cpp       # ESC protocol
│   └── TmotorTelemetry.h/cpp # Telemetry aggregation
│
├── Xag/                  # PWM-only (XAG build)
│   └── XagTelemetry.h/cpp    # Telemetry from analog sensors
│
├── Telemetry/            # Unified telemetry facade
│   ├── Telemetry.h/cpp       # Delegates to *Telemetry
│   ├── TelemetryFormatter.h  # formatVoltage, formatTemperature
│   └── TelemetryData.h       # Struct (no logic)
│
├── Sensors/              # Agnostic sensors (XAG)
│   └── BatteryVoltageSensor.h/cpp
│
├── Temperature/          # NTC sensor (ReadFn + adcVoltageRef)
├── Throttle/             # Throttle control (ReadFn)
├── ADS1115/              # I2C ADC (Hobbywing/Tmotor)
├── Xctod/                # Bluetooth LE telemetry output
├── WebServer/            # WiFi configuration
└── Logger/               # CSV logging
```

## Component Details

### 1. **main.cpp** - Application Core
The main application loop coordinates all system components.

**Key Functions:**
- `setup()`: System initialization, CAN bus (TWAI) setup, ESC announcement.
- `loop()`: Main execution loop calling all component handlers.
- `handleEsc()`: Applies calculated power limits and sends PWM to the ESC.
- `checkCanbus()`: Polls the CAN bus and routes messages.
- `isMotorRunning()`: Detects motor operation via RPM or throttle position.
- `handleArmedBeep()`: Continuous beep alert when armed but the motor is stopped.
- `handleButtonEvent()`: AceButton callback for button events.

### 2. **config.h/config.cpp** - Configuration Management
Centralized configuration with all constants and global object instances.

**Critical Constants:**
- **Battery:** Min/max voltage managed by Settings (configurable).
- **Motor:** Max temp, reduction start managed by Settings.
- **ESC:** Max temp, reduction start managed by Settings. PWM range: 1050-1950μs (Hobbywing), 1100-1940μs (Tmotor), 1130-2000μs (XAG).
- **Throttle:** ReadFn (ADS1115 or analogRead). Automatic calibration, no fixed range.

**Global Objects:**
All component instances are declared as `extern` in `config.h` and instantiated in `config.cpp`.

### 3. **Throttle/** - Throttle Control System
Manages throttle input via `ReadFn` (ADS1115 or analogRead) with automatic calibration and filtering.

**Features:**
- **Automatic Calibration:** A 3-second calibration procedure on startup automatically detects the sensor's min/max range.
- **Noise Filtering:** 30-sample moving average filter.
- **Arming System:** Safety arming/disarming mechanism.
- **Cruise Control:** Maintains throttle position when activated.
- **Sensor Validation:** Detects sensor failures and invalid readings.

### 4. **HobbywingCan** - DroneCAN ESC Interface
Implements the DroneCAN protocol for Hobbywing ESC communication.

**Telemetry (Received from ESC):**
- **Status Message 1 (0x4E52):** RPM, motor direction.
- **Status Message 2 (0x4E53):** Voltage, current.
- **Status Message 3 (0x4E54):** ESC temperature.

**Control Commands (Sent to ESC):**
- Announce, LED Control, Direction, Throttle Source, Raw Throttle, ESC ID Request.

**HobbywingTelemetry** aggregates HobbywingCan + motorTemp (ADS1115). Similar: **TmotorCan**, **TmotorTelemetry** (UAVCAN); **XagTelemetry** (analog sensors).

### 5. **Power/** - Intelligent Power Management
Calculates available power based on battery voltage, motor temperature, and ESC temperature. Uses `telemetry.getXxx()` (Telemetry facade).

**Power Limiting Factors:**
1. **Battery Voltage:** Progressive reduction below nominal voltage.
2. **Motor Temperature:** Linear reduction from 50°C to 60°C.
3. **ESC Temperature:** Linear reduction from 80°C to 110°C.
4. **Battery Power Floor:** Progressive ramp-up to protect the battery from sudden loads.

### 6. **Temperature/** - Thermal Monitoring
Reads an NTC thermistor for motor temperature monitoring. Agnostic: uses `ReadFn` + `adcVoltageRef`.

**Specifications:**
- **Sensor Type:** 10K NTC thermistor, Beta = 3950K.
- **Filtering:** 10-sample moving average.
- **ADC Source:** ADS1115 (Hobbywing/Tmotor) or ESP32 built-in (XAG).

### 7. **Canbus/** - CAN Bus Message Handler
Provides `receive(twai_message_t* outMsg)` (non-blocking). Does not know HobbywingCan or TmotorCan.

**Responsibilities:**
- `twai_receive()` and process NodeStatus/GetNodeInfo internally.
- Return raw ESC frames to caller. **main.cpp** routes frames to `hobbywingCan.parseEscMessage()` or `tmotorCan.parseEscMessage()`.

### 8. **Button/** - User Input Interface
Handles single-button input using the AceButton library.

**Button Events:**
- **Single Click:** Arm/disarm the throttle system.
- **Long Press:** Activate cruise control.

### 9. **Buzzer/** - Audible Feedback System
Provides audio feedback for system events using a passive buzzer controlled by PWM.

**Beep Types:**
- Success, Error, Warning, Custom, and a continuous alert for when the system is armed but the motor is stopped.
- All beep patterns are non-blocking.

### 10. **Xctod/** - Bluetooth LE Telemetry
Outputs comprehensive system telemetry via Bluetooth LE. Uses `telemetry.getXxx()` (Telemetry facade).

**Functionality:**
- **Protocol:** Transmits data in a format compatible with the **XCTRACK** application.
- **Update Rate:** 1-second intervals.
- **Information Transmitted:** Battery status, throttle, motor/ESC data, and overall system status.

## Core Functionality & Operation Flow

### System Initialization (setup())
1. **Buzzer & XCTOD Setup:** Initialize audio feedback and BLE telemetry.
2. **CAN Bus (TWAI) Setup:** Configure the TWAI driver (500 kbps Hobbywing, 1 Mbps T-Motor).
3. **ESC Configuration:** Announce controller presence, request ESC ID, set throttle source to PWM, and set LED to green.
4. **ESC PWM:** Attach the servo object to the designated GPIO pin.

### Main Operation Loop (loop())
1. **Button Check:** Process user input events.
2. **CAN Bus Check:** `while (canbus.receive(&msg))` routes ESC frames to hobbywingCan/tmotorCan; calls handle(); `canbus.sendNodeStatus()`.
3. **Component Handling:** Update throttle, motorTemp, escTemp (XAG), batterySensor (XAG), buzzer.
4. **Telemetry:** `telemetry.update()` (facade delegates to HobbywingTelemetry/TmotorTelemetry/XagTelemetry).
5. **ESC Output:** Calculate and apply power limits, then write the PWM signal.

### Safety Features
- **Voltage Protection:** Progressively reduces power to prevent battery over-discharge.
- **Thermal Protection:** Reduces power based on motor and ESC temperature to prevent overheating.
- **Arming Safety:** Requires explicit arming and provides an audible alert if armed while the motor is stopped.
- **Sensor Validation:** Includes checks for temperature sensor range and CAN communication timeouts.

## Development Guidelines

### Building the Project

**IMPORTANT FOR AI ASSISTANTS:**
- The project uses **PlatformIO** as the build system
- **DO NOT** attempt to run `pio` or `platformio` commands directly - they may not be in PATH
- The build is typically executed via:
  - **VS Code PlatformIO Extension** (recommended - users typically use this)
  - **PlatformIO IDE**
  - **PlatformIO Core** installed via Python (if available)

  Use: `~/.platformio/penv/bin/pio run`

**Build Process:**
- The build system is configured in `platformio.ini`
- Build outputs: `.pio/build/<env>/` (e.g., `lolin_c3_mini_hobbywing`)
- The project builds successfully when all dependencies are resolved
- To verify compilation, check for build errors in the IDE output

**Build Configuration:**
- **Environments:** `lolin_c3_mini_hobbywing`, `lolin_c3_mini_tmotor`, `lolin_c3_mini_xag`
- **Platform:** Espressif 32 (ESP32-C3)
- **Framework:** Arduino
- **Build flags:** `CONTROLLER_TYPE=1` (XAG), `2` (Hobbywing), `3` (Tmotor)

**When checking if code compiles:**
- Use the linter to check for syntax errors (`read_lints` tool)
- If build verification is needed, inform the user to build via their IDE
- Do NOT attempt to run PlatformIO commands unless explicitly requested

### Adding New Components
1. **Create Component Directory:** `src/NewComponent/`.
2. **Define Class Interface:** Create `.h` and `.cpp` files.
3. **Register in `config.h`:** Add `extern` declaration.
4. **Instantiate in `config.cpp`:** Create the global object instance.
5. **Integrate in `main.cpp`:** Call its `setup()` and `handle()` methods.

### Coding Best Practices
- **Language:** All code and comments in English.
- **Naming:** Use clear, descriptive names (camelCase for functions, PascalCase for classes).
- **Constants:** Use `#define` or `const` for magic numbers.
- **Non-blocking:** Never use `delay()` in the main loop; use timestamp-based timing (`millis()`).

### Troubleshooting Guide

**ESC Not Responding:**
- Check CAN bus wiring (H, L) to the transceiver.
- Hobbywing: 500kbps; Tmotor: 1Mbps. Verify bitrate match.
- Ensure the ESC is in DroneCAN (Hobbywing) or UAVCAN (Tmotor) mode.
- Monitor `hobbywingCan.isReady()` or `tmotorCan.isReady()`.

**Throttle Not Calibrating:**
- Ensure the Hall sensor is moved through its full range during startup.
- Check sensor connections to GPIO 0.

**Temperature Reading Invalid:**
- Check NTC sensor connections to GPIO 1.
- Verify the 10K thermistor specification.

## References & Resources

- **ArduPilot DroneCAN Implementation:** https://github.com/ArduPilot/ardupilot
- **AceButton Library:** https://github.com/bxparks/AceButton
- **ESP32-C3 Datasheet:** Official documentation from Espressif.

---

**Project Repository:** https://github.com/rodrigomedeirosbrazil/fly-controller
**Developer:** Rodrigo Medeiros
**License:** MIT

*This document is for AI assistants to understand the codebase architecture and assist with development tasks.*
