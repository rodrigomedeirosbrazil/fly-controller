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
- **CAN Bus Speed:** 500 kbps
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
│
├── Button/               # User input handling
│   ├── Button.h
│   └── Button.cpp
│
├── Buzzer/               # Audible feedback system (PWM)
│   ├── Buzzer.h
│   └── Buzzer.cpp
│
├── Canbus/               # CAN bus message routing (TWAI)
│   ├── Canbus.h
│   └── Canbus.cpp
│
├── Hobbywing/            # DroneCAN ESC communication
│   ├── Hobbywing.h
│   └── Hobbywing.cpp
│
├── Power/                # Intelligent power management
│   ├── Power.h
│   └── Power.cpp
│
├── Temperature/          # NTC temperature sensor
│   ├── Temperature.h
│   └── Temperature.cpp
│
├── Throttle/             # Throttle control & calibration
│   ├── Throttle.h
│   └── Throttle.cpp
│
└── Xctod/                # Bluetooth LE telemetry output
    ├── Xctod.h
    └── Xctod.cpp
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
- **Battery:**
  - Min voltage: 420dV (42.0V) = 3.0V/cell
  - Max voltage: 574dV (57.4V) = 4.1V/cell

- **Motor:**
  - Max temperature: 60°C
  - Temperature reduction starts: 50°C
- **ESC:**
  - Max temperature: 110°C
  - Temperature reduction starts: 80°C
  - PWM range: 1050-1950μs
- **Throttle:**
  - Hall sensor pin: GPIO 0
  - Automatic calibration, no fixed range.

**Global Objects:**
All component instances are declared as `extern` in `config.h` and instantiated in `config.cpp`.

### 3. **Throttle/** - Throttle Control System
Manages throttle input from a Hall sensor with automatic calibration and filtering.

**Features:**
- **Automatic Calibration:** A 3-second calibration procedure on startup automatically detects the sensor's min/max range.
- **Noise Filtering:** 30-sample moving average filter.
- **Arming System:** Safety arming/disarming mechanism.
- **Cruise Control:** Maintains throttle position when activated.
- **Sensor Validation:** Detects sensor failures and invalid readings.

### 4. **Hobbywing/** - DroneCAN ESC Interface
Implements the DroneCAN protocol for Hobbywing ESC communication.

**Telemetry (Received from ESC):**
- **Status Message 1 (0x4E52):** RPM, motor direction.
- **Status Message 2 (0x4E53):** Voltage, current.
- **Status Message 3 (0x4E54):** ESC temperature.

**Control Commands (Sent to ESC):**
- Announce, LED Control, Direction, Throttle Source, Raw Throttle, ESC ID Request.

### 5. **Power/** - Intelligent Power Management
Calculates available power based on battery voltage, motor temperature, and ESC temperature.

**Power Limiting Factors:**
1. **Battery Voltage:** Progressive reduction below nominal voltage.
2. **Motor Temperature:** Linear reduction from 50°C to 60°C.
3. **ESC Temperature:** Linear reduction from 80°C to 110°C.
4. **Battery Power Floor:** Progressive ramp-up to protect the battery from sudden loads.

### 6. **Temperature/** - Thermal Monitoring
Reads an NTC thermistor for motor temperature monitoring.

**Specifications:**
- **Sensor Type:** 10K NTC thermistor, Beta = 3950K.
- **Filtering:** 10-sample moving average.
- **Pin:** GPIO 1.

### 7. **Canbus/** - CAN Bus Message Router
A central router for all CAN bus messages, using the ESP32's built-in TWAI controller.

**Responsibilities:**
- Parse incoming CAN frames.
- Identify message source (e.g., Hobbywing ESC).
- Route messages to the correct component handler.

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
Outputs comprehensive system telemetry via Bluetooth LE for monitoring and debugging.

**Functionality:**
- **Protocol:** Transmits data in a format compatible with the **XCTRACK** application.
- **Update Rate:** 1-second intervals.
- **Information Transmitted:** Battery status, throttle, motor/ESC data, and overall system status.

## Core Functionality & Operation Flow

### System Initialization (setup())
1. **Buzzer & XCTOD Setup:** Initialize audio feedback and BLE telemetry.
2. **CAN Bus (TWAI) Setup:** Configure the TWAI driver for 500kbps.
3. **ESC Configuration:** Announce controller presence, request ESC ID, set throttle source to PWM, and set LED to green.
4. **ESC PWM:** Attach the servo object to the designated GPIO pin.

### Main Operation Loop (loop())
1. **Button Check:** Process user input events.
2. **CAN Bus Check:** Read and parse incoming messages.
3. **Component Handling:** Update throttle, temperature, and buzzer states.
4. **ESC Output:** Calculate and apply power limits, then write the PWM signal.
5. **Telemetry:** The `Xctod` component handles BLE transmissions periodically.

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
- Build outputs are generated in `.pio/build/lolin_c3_mini/`
- The project builds successfully when all dependencies are resolved
- To verify compilation, check for build errors in the IDE output

**Build Configuration:**
- **Environment:** `lolin_c3_mini`
- **Platform:** Espressif 32 (ESP32-C3)
- **Framework:** Arduino
- **Build flags:** Defined in `platformio.ini` (e.g., `-D ELEGANTOTA_USE_ASYNC_WEBSERVER=1`)
- **Mode XAG:** Add `-D XAG=1` to `build_flags` in `platformio.ini` for XAG motor support

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
- Verify the 500kbps bitrate match.
- Ensure the ESC is in DroneCAN mode.
- Monitor `hobbywing.isReady()` status.

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
