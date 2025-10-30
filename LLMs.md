# Fly Controller - AI Assistant Guide

## Overview

**Fly Controller** is an advanced Arduino-based firmware for an intelligent flight controller system designed for drones and UAVs. Built on the PlatformIO framework for the ATmega328P microcontroller, it provides comprehensive power management, battery monitoring, and ESC communication using the DroneCAN protocol.

**Code Standards:**
- All code MUST be written in English, including all comments
- Follow Arduino/C++ best practices
- Maintain modular architecture with clear separation of concerns
- Document complex algorithms and protocol implementations

## Key Technologies & Libraries

### Framework & Platform
- **Framework:** Arduino
- **Platform:** Atmel AVR (specifically `pro16MHzatmega328` @ 16MHz)
- **Build System:** PlatformIO

### Core Libraries
- **`arduino-libraries/Servo@^1.2.2`**: PWM signal generation for ESC control
- **`autowp/autowp-mcp2515@^1.2.1`**: CAN bus communication via MCP2515 controller
- **`bxparks/AceButton@^1.10.1`**: Advanced button handling with event detection

### Simulation
The project supports simulation using **Wokwi**, configured via `wokwi.toml`. The simulation environment includes virtual hardware components for testing without physical hardware.

## Project Architecture

### Hardware Configuration
- **Microcontroller:** ATmega328P (16MHz) - Arduino Nano/Pro Mini compatible
- **CAN Controller:** MCP2515 @ 8MHz with SPI interface
- **CAN Bus Speed:** 500 kbps
- **PWM Output:** ESC control (1050-1950μs)
- **Analog Inputs:**
  - A0: NTC temperature sensor (motor)
  - A1: Hall sensor (throttle input)
- **Digital I/O:**
  - Pin 3: Push button (with interrupt capability)
  - Pin 4: Buzzer output
  - Pin 9: ESC PWM output
  - Pins 10-13: SPI for MCP2515 (CS, MOSI, MISO, SCK)

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
├── Buzzer/               # Audible feedback system
│   ├── Buzzer.h
│   └── Buzzer.cpp
│
├── Canbus/               # CAN bus message routing
│   ├── Canbus.h
│   └── Canbus.cpp
│
├── Hobbywing/            # DroneCAN ESC communication
│   ├── Hobbywing.h
│   └── Hobbywing.cpp
│

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
└── Xctod/                # Serial telemetry output
    ├── Xctod.h
    └── Xctod.cpp
```

## Component Details

### 1. **main.cpp** - Application Core
Main application loop coordinating all system components.

**Key Functions:**
- `setup()`: System initialization, CAN bus setup, ESC announcement
- `loop()`: Main execution loop calling all component handlers
- `handleEsc()`: Applies calculated power limits and sends PWM to ESC
- `checkCanbus()`: Polls CAN bus and routes messages
- `isMotorRunning()`: Detects motor operation via RPM or throttle position
- `handleArmedBeep()`: Continuous beep alert when armed but motor stopped
- `handleButtonEvent()`: AceButton callback for button events

**Execution Flow:**
1. Initialize all components (Xctod, Buzzer, CAN bus)
2. Configure and announce to Hobbywing ESC
3. Main loop: button check → display update → CAN bus check → throttle handling → temperature monitoring → buzzer handling → ESC output

### 2. **config.h/config.cpp** - Configuration Management
Centralized configuration with all constants and global object instances.

**Critical Constants:**
- **Battery:**
  - Min voltage: 420dV (42.0V) = 3.0V/cell
  - Max voltage: 574dV (57.4V) = 4.1V/cell
  - Max current: 140A (peak)
  - Continuous current: 105A

- **Motor:**
  - Max temperature: 60°C
  - Temperature reduction starts: 50°C
  - Temperature sensor: NTC on A0

- **ESC:**
  - Max temperature: 110°C
  - Temperature reduction starts: 80°C
  - Max current: 200A
  - Continuous current: 80A
  - PWM range: 1050-1950μs

- **Throttle:**
  - Hall sensor pin: A1
  - Default range: 170-850 (auto-calibration available)
  - Recovery percentage: 25%

**Global Objects:**
All component instances are declared as extern in `config.h` and instantiated in `config.cpp`.

### 3. **Throttle/** - Throttle Control System
Manages throttle input from Hall sensor with automatic calibration and filtering.

**Features:**
- **Automatic Calibration:** 3-second calibration procedure on startup
- **Noise Filtering:** 30-sample moving average filter
- **Arming System:** Safety arming/disarming mechanism
- **Cruise Control:** Maintains throttle position when activated
- **Sensor Validation:** Detects sensor failures and invalid readings

**Key Methods:**
- `handle()`: Main update loop for throttle reading and calibration
- `setArmed()` / `setDisarmed()`: Control arming state
- `getThrottlePercentage()`: Returns 0-100% throttle position
- `getThrottleRaw()`: Returns raw filtered analog value
- `isArmed()`: Returns current arming state
- `isCalibrated()`: Returns calibration status

**Calibration Process:**
1. Move throttle to maximum position
2. Hold for 3 seconds
3. Move to minimum position
4. System automatically detects ranges and completes calibration

### 4. **Hobbywing/** - DroneCAN ESC Interface
Complete implementation of DroneCAN protocol for Hobbywing X13 ESC communication.

**Protocol Implementation:**
- **DroneCAN Message Format:** CAN 2.0B extended frames with DroneCAN payload structure
- **Node IDs:** Controller=0x13, ESC=0x03
- **Service/Message Types:** Status broadcasts, configuration services, control commands

**Telemetry (Received from ESC):**
- **Status Message 1 (0x4E52):** RPM, motor direction (CCW/CW)
- **Status Message 2 (0x4E53):** Voltage (decivolts), current (deciamps)
- **Status Message 3 (0x4E54):** ESC temperature (°C)

**Control Commands (Sent to ESC):**
- **Announce:** Periodic node presence broadcast
- **LED Control:** RGB color and blink pattern (off, 1Hz, 2Hz, 5Hz)
- **Direction:** Set motor rotation (CW/CCW)
- **Throttle Source:** Select PWM or CAN bus control
- **Raw Throttle:** Direct throttle command via CAN
- **ESC ID Request:** Discover ESC node ID

**Key Methods:**
- `parseEscMessage()`: Main parser for all ESC CAN messages
- `announce()`: Send periodic presence announcement (called every loop)
- `setLedColor()`: Control ESC LED (red, green, blue, combinations)
- `setThrottleSource()`: Switch between PWM and CAN throttle control
- `requestEscId()`: Request ESC to report its node ID
- `isReady()`: Check if ESC is communicating (telemetry timeout check)
- Getters: `getRpm()`, `getDeciVoltage()`, `getDeciCurrent()`, `getTemperature()`

**DroneCAN Frame Structure:**
- Priority (3 bits)
- Data Type ID or Service Type ID (16 bits)
- Source Node ID (7 bits)
- Destination Node ID (7 bits for services)
- Transfer ID (5 bits, in payload tail byte)



### 6. **Power/** - Intelligent Power Management
Calculates available power based on battery voltage, motor temperature, and ESC temperature.

**Power Limiting Factors:**
1. **Battery Voltage:** Progressive reduction below nominal voltage
2. **Motor Temperature:** Linear reduction from 50°C to 60°C
3. **ESC Temperature:** Linear reduction from 80°C to 110°C
4. **Battery Power Floor:** Progressive ramp-up to protect battery from sudden loads

**Algorithm:**
- Reads throttle percentage from Throttle component
- Applies battery voltage limit
- Applies motor temperature limit (if temperature sensor valid)
- Applies ESC temperature limit (if ESC connected)
- Applies battery power floor (gradual power ramp-up)
- Converts final power percentage to PWM microseconds (1050-1950μs)

**Key Methods:**
- `getPwm()`: Returns calculated PWM value for ESC (updates every 10ms)
- `getPower()`: Returns current calculated power percentage (0-100%)
- `resetBatteryPowerFloor()`: Resets progressive power ramp (called on disarm)

**Protection Strategy:**
The power management implements multiple layers of protection to prevent component damage while maintaining smooth operation. Temperature-based limitations are progressive, not binary, allowing for graceful degradation.

### 7. **Temperature/** - Thermal Monitoring
Reads NTC thermistor for motor temperature monitoring.

**Specifications:**
- **Sensor Type:** 10K NTC thermistor, Beta = 3950K
- **Filtering:** 10-sample moving average
- **Range:** -10°C to 150°C valid range
- **Update Rate:** Reads on every `handle()` call

**Key Methods:**
- `handle()`: Read and filter temperature sensor
- `getTemperature()`: Returns filtered temperature in °C
- `isValid()`: Checks if reading is within valid range

**Temperature Calculation:**
Uses Steinhart-Hart equation for accurate NTC temperature conversion with ambient temperature compensation.

### 8. **Canbus/** - CAN Bus Message Router
Central router for all CAN bus messages, directing them to appropriate components.

**Responsibilities:**
- Parse incoming CAN frames
- Identify message source (Hobbywing ESC)
- Route messages to correct component handler
- Provide debug logging capabilities

**Key Methods:**
- `parseCanMsg()`: Main routing function
  - Detects Hobbywing messages (data type IDs 0x4E52-0x4E56)
  - Detects JK-BMS messages (IDs 0x100-0x103)
  - Can be extended for additional devices

**Architecture Pattern:**
The Canbus class acts as a dispatcher, allowing new CAN devices to be added without modifying existing component code. Each device has its own parser class.

### 9. **Button/** - User Input Interface
Handles single button input using AceButton library for event detection.

**Button Events:**
- **Single Click:** Arm/disarm throttle system
- **Long Press:** Activate cruise control (when armed and throttle > 30%)
- **Double Click:** (Reserved for future features)

**Key Methods:**
- `check()`: Must be called every loop iteration
- `handleEvent()`: Processes button events
  - Arms/disarms throttle
  - Activates cruise control
  - Provides buzzer feedback

**Integration:**
Uses `handleButtonEvent()` callback in `main.cpp` to handle AceButton events.

### 10. **Buzzer/** - Audible Feedback System
Provides audio feedback for system events and warnings.

**Beep Types:**
- **Success Beep:** Short beep for successful operations (arming)
- **Error Beep:** Rapid beeps for errors or failures
- **Warning Beep:** Medium beeps for warnings
- **Custom Beep:** Configurable duration and repetitions
- **Continuous Beep:** Alert when armed but motor stopped

**Key Methods:**
- `setup()`: Initialize buzzer pin
- `handle()`: Update buzzer state (must be called every loop)
- `beepSuccess()`: Confirmation beep
- `beepError()`: Error indication
- `beepWarning()`: Warning alert
- `beepCustom(duration, repetitions)`: Custom pattern
- `stop()`: Stop any ongoing beep

**Non-blocking Design:**
All beep patterns are non-blocking, using `handle()` to update state without blocking the main loop.

### 11. **Xctod/** - Serial Telemetry Display
Outputs comprehensive system telemetry via Serial port for monitoring and debugging.

**Output Information:**
- **Battery:** Voltage, current, SOC, temperature, cell voltages
- **Throttle:** Percentage, armed state, calibration status
- **Motor:** Temperature, RPM
- **ESC:** Temperature, voltage, current from ESC telemetry
- **Power:** Calculated power limit and PWM output
- **System Status:** Overall system health and warnings

**Configuration:**
- **Baud Rate:** 9600 (configurable in `init()`)
- **Update Rate:** 1 second intervals
- **Format:** Human-readable text output

**Key Methods:**
- `init(baudRate)`: Initialize serial communication
- `write()`: Output telemetry data (updates every 1 second)

**Usage:**
Connect serial monitor to view real-time telemetry. Useful for debugging, tuning, and system monitoring.

## Core Functionality & Operation Flow

### System Initialization (setup())
1. **Serial Initialization:** Start Xctod display
2. **Buzzer Setup:** Initialize audio feedback
3. **CAN Bus Setup:** Reset MCP2515, set 500kbps bitrate, enable normal mode
4. **ESC Configuration:**
   - Announce controller presence
   - Request ESC ID
   - Set throttle source to PWM
   - Set LED to green
5. **Warning Beep:** Audio indication of successful initialization
6. **ESC PWM:** Attach servo object and set to minimum PWM

### Main Operation Loop (loop())
1. **Button Check:** Process user input events
2. **Display Update:** Output telemetry to serial (1 second intervals)
3. **CAN Bus Check:** Read and parse incoming messages
4. **Throttle Handling:** Update throttle reading and calibration
5. **Temperature Monitoring:** Read and filter motor temperature
6. **Buzzer Handling:** Update audible feedback state
7. **ESC Output:** Calculate and apply power limits, write PWM
8. **Armed Beep:** Continuous alert if armed but motor not running

### Safety Features

**Voltage Protection:**
- Monitors battery voltage from ESC or BMS
- Progressively reduces power below minimum voltage
- Prevents over-discharge damage

**Thermal Protection:**
- **Motor Temperature:** Reduces power from 50°C, limits at 60°C
- **ESC Temperature:** Reduces power from 80°C, limits at 110°C
- Prevents overheating damage

**Current Protection:**
- Battery max current: 140A peak, 105A continuous
- ESC max current: 200A peak, 80A continuous
- Monitored via ESC and BMS telemetry

**Arming Safety:**
- System must be explicitly armed before motor operation
- Auto-disarm on critical failures
- Continuous beep when armed but motor stopped (safety alert)

**Sensor Validation:**
- Temperature sensor range checking
- Throttle sensor calibration and validation
- CAN communication timeout detection

### Communication Protocols

**DroneCAN (Hobbywing ESC):**
- **Speed:** 500 kbps CAN bus
- **Protocol:** DroneCAN over CAN 2.0B extended frames
- **Message Structure:** Priority + Data Type + Node IDs + Payload + Transfer ID
- **Periodic Announcements:** Controller announces presence every loop
- **Bidirectional:** Both telemetry reception and control commands



**PWM ESC Control:**
- **Signal:** Standard servo PWM (50Hz)
- **Range:** 1050μs (min) to 1950μs (max)
- **Resolution:** Microsecond precision via Servo library
- **Failsafe:** Automatically outputs minimum PWM when disarmed

## Development Guidelines

### Adding New Components

1. **Create Component Directory:**
   ```
   src/NewComponent/
   ├── NewComponent.h
   └── NewComponent.cpp
   ```

2. **Define Class Interface (NewComponent.h):**
   - Clear public API
   - Private implementation details
   - Documentation comments

3. **Implement Functionality (NewComponent.cpp):**
   - Constructor initialization
   - Main update method (usually `handle()`)
   - Getter/setter methods

4. **Register in config.h:**
   - Add forward declaration
   - Add extern declaration
   - Define related constants

5. **Instantiate in config.cpp:**
   - Create global object instance

6. **Integrate in main.cpp:**
   - Include header
   - Call initialization in `setup()`
   - Call update method in `loop()`

### Coding Best Practices

- **Language:** All code and comments in English
- **Naming:** Clear, descriptive names (camelCase for variables/functions, PascalCase for classes)
- **Constants:** Use `#define` or `const` for magic numbers
- **Comments:** Document complex algorithms, protocols, and non-obvious logic
- **Modularity:** Keep components loosely coupled
- **Non-blocking:** Never use `delay()` in main loop - use timestamp-based timing
- **Memory:** Be mindful of AVR limited RAM (2KB) - avoid dynamic allocation
- **Globals:** Use extern pattern in config.h for shared objects

### Testing & Simulation

**Wokwi Simulation:**
- Build project: `pio run`
- Upload firmware hex to Wokwi simulator
- Virtual components available for testing without hardware

**Hardware Testing:**
- Always test safety features (voltage, temperature limits)
- Verify CAN bus communication before flight
- Calibrate throttle sensor in actual installation
- Check buzzer feedback for all scenarios

## Common Modifications

### Adjusting Protection Limits
Edit values in `config.h`:
- Battery voltage limits
- Temperature thresholds
- Current limits
- PWM range

### Adding CAN Devices
1. Create new component class (e.g., `NewBms/`)
2. Implement message parser
3. Add message ID detection in `Canbus::parseCanMsg()`
4. Route messages to new component

### Changing Pin Assignments
Update pin definitions in `config.h`:
- Analog inputs (A0, A1)
- Digital I/O (button, buzzer, ESC PWM)
- SPI pins (MCP2515)

### Customizing Beep Patterns
Modify beep types in `Buzzer/` component:
- Duration and repetition counts
- Add new beep patterns as needed

## Troubleshooting Guide

**ESC Not Responding:**
- Check CAN bus wiring (H, L, GND)
- Verify 500kbps bitrate match
- Confirm ESC DroneCAN mode enabled
- Monitor `hobbywing.isReady()` status

**Throttle Not Calibrating:**
- Move throttle to full range during 3-second calibration
- Check Hall sensor connections
- Verify A1 analog reading in serial output

**Temperature Reading Invalid:**
- Check NTC sensor connections
- Verify 10K thermistor specification
- Check reading against valid range (-10°C to 150°C)



## References & Resources

- **ArduPilot DroneCAN Implementation:** https://github.com/ArduPilot/ardupilot
- **MCP2515 Documentation:** https://github.com/autowp/arduino-mcp2515
- **AceButton Library:** https://github.com/bxparks/AceButton
- **Wokwi Simulator:** https://wokwi.com

---

**Project Repository:** https://github.com/rodrigomedeirosbrazil/fly-controller
**Developer:** Rodrigo Medeiros
**License:** MIT

*This document is for AI assistants to understand the codebase architecture and assist with development tasks.*
