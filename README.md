# Fly Controller

**An intelligent and modular flight controller for drones and UAVs**

Fly Controller is an advanced firmware that implements a complete flight control system with DroneCAN communication and intelligent power management. Specifically designed to work with Hobbywing ESCs and run on the ESP32-C3 platform.

## 📋 Table of Contents

- [Overview](#overview)
- [System Architecture](#system-architecture)
- [Main Components](#main-components)
- [Communication Protocols](#communication-protocols)
- [Features](#features)
- [Supported Hardware](#supported-hardware)
- [Installation and Configuration](#installation-and-configuration)
- [Contributing](#contributing)
- [License](#license)

## 🚁 Overview

Fly Controller is a modular ESP32-based flight control system that offers:

- **Intelligent Throttle Control**: With automatic calibration, cruise control, and safety protections.
- **DroneCAN Communication**: Complete interface with Hobbywing X-series ESCs.
- **Bluetooth LE Telemetry**: Real-time data transmission to monitoring apps like XCTRACK via the Xctod module.
- **Power Management**: Dynamic power calculation based on battery voltage and component temperature.
- **User Interface**: Control button and a PWM-driven passive buzzer for audio feedback.
- **Complete Telemetry**: Serial output for real-time monitoring and debugging.

## 🏗️ System Architecture

### Microcontroller
- **Platform**: ESP32-C3 Super Mini
- **Framework**: Arduino
- **Build System**: PlatformIO

### Communication
- **CAN Bus**: Built-in ESP32 TWAI controller for DroneCAN.
- **Bluetooth LE**: For telemetry transmission.
- **Serial**: Telemetry and debug (115200 baud).
- **PWM**: Direct ESC control.

### Analog Inputs
- **Hall Sensor**: Throttle control (GPIO 0)
- **NTC Sensor**: Motor temperature (GPIO 1)

### Digital I/O
- **ESC PWM**: GPIO 7 (1050-1950μs)
- **Buzzer**: GPIO 6 (Passive, PWM controlled)
- **Button**: GPIO 5 (with interrupt)
- **CAN TX/RX**: GPIO 2/3

## 🔧 Main Components

### 1. **Throttle** - Throttle Control
```cpp
// Main features
- Automatic Hall sensor calibration on startup
- System arming/disarming
- Cruise control mode
- Noise filtering with moving average
- Sensor failure protection
```

**Technical Specifications:**
- Hall sensor reading is calibrated automatically.
- 30 samples for filtering.
- Automatic calibration completes in 3 seconds.
- Cruise mode activated with 30%+ throttle.

### 2. **HobbywingCan** - DroneCAN Interface
```cpp
// DroneCAN protocol for Hobbywing ESCs
- Complete telemetry (RPM, voltage, current, temperature)
- LED and motor direction control
- Throttle source configuration
- Automatic ESC ID discovery
```

**Supported Messages:**
- `0x4E52`: Status 1 (RPM, direction)
- `0x4E53`: Status 2 (voltage, current)
- `0x4E54`: Status 3 (temperature)
- LED control (red, green, blue)
- Direction and throttle commands

*T-Motor uses TmotorCan (UAVCAN); XAG uses XagTelemetry with analog sensors.*

### 3. **Power** - Power Management
```cpp
// Intelligent available power calculation
- Battery voltage-based limitation
- Motor thermal protection
- Battery power floor
- ESC PWM conversion
- Throttle ramp limiting (smooth acceleration/deceleration)
```

**Protection Algorithms:**
- Minimum voltage limitation (42.0V)
- Thermal protection (60°C motor, 110°C ESC)
- Progressive power reduction based on motor and ESC temperature
- Throttle ramp limiting: 4 μs/tick acceleration, 8 μs/tick deceleration (prevents jerky movements)

### 4. **Temperature** - Thermal Monitoring
```cpp
// Agnostic NTC sensor (ReadFn + adcVoltageRef)
- Beta = 3950K for 10K NTC
- Filtering with 10 samples
- ADC source: ADS1115 (Hobbywing/Tmotor) or ESP32 built-in (XAG)
```

### 5. **Canbus** - Communication Management
```cpp
// CAN message handling using ESP32 TWAI controller
- receive() non-blocking API - returns raw ESC frames
- main.cpp routes frames to HobbywingCan/TmotorCan
- NodeStatus and GetNodeInfo handled internally
- sendNodeStatus() for periodic announcements
```

### 6. **Buzzer** - Audio Interface
```cpp
// Sonorous alert system with a passive buzzer
- Success, error, and warning beeps
- Customizable beeps via PWM control
- Continuous alert when motor stops
```

### 7. **Button** - User Interface
```cpp
// Single button control
- Single click: Arming/Disarming
- Long press: Cruise activation
- Tactile feedback with buzzer
```

### 8. **Xctod** - Bluetooth LE Telemetry
```cpp
// Data output for monitoring via BLE
- Transmits telemetry data compatible with the XCTRACK app.
- Battery information
- Throttle status
- Motor and ESC data
- Overall system status
```

## 📡 Communication Protocols

### CAN Bus Protocols

#### DroneCAN (Hobbywing ESC)
- **Speed**: 500 kbps
- **Node ID**: 0x13 (controller), 0x03 (ESC)
- **Messages**: Status, control, configuration
- **Format**: CAN 2.0B with DroneCAN payload
- **Telemetry**: RPM, voltage, current, temperature, LED control

#### UAVCAN (T-Motor ESC)
- **Speed**: 1 Mbps
- **Protocol**: UAVCAN v0
- **Messages**: Status, control, configuration
- **Format**: CAN 2.0B with UAVCAN payload
- **Telemetry**: RPM, voltage, current, temperature

### Bluetooth LE (XCTOD)
- Provides a service to transmit flight data to compatible mobile applications (XCTRACK).
- Available in all controller modes.

## ⚡ Features

### Safety
- ✅ Low voltage protection
- ✅ Motor thermal limitation
- ✅ ESC thermal limitation
- ✅ Auto-disarm on failure
- ✅ Sonorous alert when motor stops

### Control
- ✅ Automatic throttle calibration
- ✅ Intelligent cruise control
- ✅ Button arming/disarming
- ✅ Direct ESC PWM control
- ✅ Smooth throttle ramp limiting (acceleration/deceleration control)
- ✅ Bidirectional CAN bus communication (DroneCAN/UAVCAN)

### Monitoring
- ✅ Bluetooth LE telemetry for apps (XCTRACK)
- ✅ Complete serial telemetry
- ✅ Real-time status
- ✅ Event logging
- ✅ Failure diagnosis

## 🔌 Supported Hardware

### Microcontroller
- **ESP32-C3 Super Mini**

### ESC
- **Hobbywing X-Series** (DroneCAN mode - default): Full telemetry via CAN bus
- **T-Motor** (UAVCAN mode): Full telemetry via CAN bus
- **XAG Motors** (XAG mode): PWM-only control with NTC temperature sensors
- Any ESC with PWM input (compatible with all modes)

### Battery
- 14S Li-ion/Li-Po packs (configurable voltage)

### Sensors
- Hall Sensor (throttle)
- 10K NTC 3950K (temperature)

### Interface
- Push-button
- Passive piezoelectric buzzer

## 🚀 Installation and Configuration

### Build Environments

The project supports three controller types with dedicated build environments:

| Controller | Environment | Protocol | CAN Bus |
|------------|-------------|----------|---------|
| **Hobbywing** | `lolin_c3_mini_hobbywing` | DroneCAN | ✅ Required |
| **T-Motor** | `lolin_c3_mini_tmotor` | UAVCAN | ✅ Required |
| **XAG** | `lolin_c3_mini_xag` | PWM-only | ❌ Not required |

**Quick Build Commands:**
```bash
# Hobbywing (default)
~/.platformio/penv/bin/pio run -e lolin_c3_mini_hobbywing

# T-Motor
~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor

# XAG
~/.platformio/penv/bin/pio run -e lolin_c3_mini_xag
```

### 1. Prerequisites
```bash
# Install PlatformIO CLI
pip install platformio

# Or use the PlatformIO IDE extension for VSCode
```

### 2. Clone Repository
```bash
git clone https://github.com/rodrigomedeirosbrazil/fly-controller.git
cd fly-controller
```

### 3. Dependencies
The required libraries are automatically managed by PlatformIO.
```ini
# platformio.ini
lib_deps =
    madhephaestus/ESP32Servo
    bxparks/AceButton@^1.10.1
```

### 4. Build and Upload

**Using VS Code PlatformIO Extension (Recommended):**
- Install the PlatformIO extension in VS Code
- Open the project folder
- Select the desired environment from the PlatformIO toolbar (bottom status bar)
- Use the PlatformIO toolbar to build, upload, and monitor

**Using PlatformIO CLI:**

The project supports three controller types, each with its own build environment:

#### Hobbywing Controller (Default - DroneCAN)
```bash
# Build for Hobbywing ESC (DroneCAN protocol)
~/.platformio/penv/bin/pio run -e lolin_c3_mini_hobbywing

# Or use the default environment
~/.platformio/penv/bin/pio run -e lolin_c3_mini

# Upload firmware
~/.platformio/penv/bin/pio run -e lolin_c3_mini_hobbywing -t upload

# Monitor serial output
~/.platformio/penv/bin/pio device monitor -e lolin_c3_mini_hobbywing
```

**Features:**
- Full CAN bus support (500 kbps)
- DroneCAN protocol communication
- Complete telemetry (RPM, voltage, current, temperature)
- LED control and ESC configuration

#### T-Motor Controller (UAVCAN)
```bash
# Build for T-Motor ESC (UAVCAN protocol)
~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor

# Upload firmware
~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor -t upload

# Monitor serial output
~/.platformio/penv/bin/pio device monitor -e lolin_c3_mini_tmotor
```

**Features:**
- Full CAN bus support (1 Mbps)
- UAVCAN protocol communication
- Complete telemetry (RPM, voltage, current, temperature)
- ESC configuration and control

#### XAG Controller (PWM-only)
```bash
# Build for XAG motors (PWM-only, no CAN bus)
~/.platformio/penv/bin/pio run -e lolin_c3_mini_xag

# Upload firmware
~/.platformio/penv/bin/pio run -e lolin_c3_mini_xag -t upload

# Monitor serial output
~/.platformio/penv/bin/pio device monitor -e lolin_c3_mini_xag
```

**Features:**
- PWM-only control (no CAN bus required)
- Analog temperature sensors (NTC)
- Battery voltage monitoring via voltage divider
- Throttle ramp limiting for smooth acceleration/deceleration

**Note:** The XAG mode includes smooth throttle ramp limiting (4 μs/tick acceleration, 8 μs/tick deceleration) to prevent jerky motor movements.

### 5. Pin Configuration
```cpp
// src/config.h - Main configurations
// Analog Inputs
#define THROTTLE_PIN          0  // GPIO0 - Hall Sensor
#define MOTOR_TEMPERATURE_PIN 1  // GPIO1 - NTC 10K
// XAG Mode only:
#define ESC_TEMPERATURE_PIN   4  // GPIO4 - NTC 10K (XAG mode)
#define BATTERY_VOLTAGE_PIN   3  // GPIO3 - Battery voltage divider (XAG mode)

// Digital I/O
#define BUTTON_PIN 5  // GPIO5 - Push button
#define BUZZER_PIN 6  // GPIO6 - Passive Buzzer (PWM)
#define ESC_PIN    7  // GPIO7 - ESC PWM signal

// CAN Bus (TWAI) - Hobbywing and T-Motor modes only
#define CAN_TX_PIN 2  // GPIO2 - CAN TX
#define CAN_RX_PIN 3  // GPIO3 - CAN RX
```

### 6. Throttle Ramp Limiting Configuration
```cpp
// src/config.h - Throttle ramp limiting parameters
#define THROTTLE_RAMP_RATE 8        // Maximum acceleration in μs/tick
#define THROTTLE_DECEL_MULTIPLIER 2.0  // Deceleration multiplier (2x faster)
```

**Behavior:**
- Acceleration: Limited to 4 μs per tick (smooth ramp-up)
- Deceleration: Limited to 8 μs per tick (2x faster, responsive braking)
- Applied to all controller types for smooth motor control

### 7. Calibration
The system automatically calibrates the Hall sensor for the throttle on every startup. No manual configuration is needed.

**Calibration Process:**
1. On startup, move throttle to maximum position and hold for 3 seconds
2. Move throttle to minimum position and hold for 3 seconds
3. Calibration complete - system is ready to arm

## 📊 Usage Example

### Operation Sequence
1. **Initialization**: Automatic throttle calibration, CAN bus setup (if applicable).
2. **Arming**: Press the button to arm the system (throttle must be at minimum).
3. **Operation**: Adjust the throttle; the system calculates and delivers safe power with smooth ramp limiting.
4. **Cruise**: Maintain throttle >30% for a few seconds to activate cruise control.
5. **Disarm**: Press the button again to disarm.

### Controller-Specific Notes

**Hobbywing/T-Motor (CAN bus):**
- Ensure CAN bus is properly connected (H/L lines)
- ESC must be configured for DroneCAN (Hobbywing) or UAVCAN (T-Motor) mode
- Full telemetry available via CAN bus

**XAG (PWM-only):**
- No CAN bus required
- Temperature sensors must be connected (motor and ESC)
- Battery voltage divider must be connected
- Throttle ramp limiting provides smooth motor control

## 🛠️ Development

### File Structure
```
src/
├── main.cpp              # Main loop
├── config.h, config.cpp  # Configuration and object instances
├── config_controller.h   # Build type (IS_HOBBYWING, IS_TMOTOR, IS_XAG)
├── Throttle/             # Throttle control (ReadFn)
├── Temperature/          # Thermal sensor (ReadFn)
├── Power/                # Power management
├── Canbus/               # CAN receive() API
├── Hobbywing/            # HobbywingCan, HobbywingTelemetry
├── Tmotor/               # TmotorCan, TmotorTelemetry
├── Xag/                  # XagTelemetry (PWM-only)
├── Sensors/              # BatteryVoltageSensor (XAG)
├── Telemetry/            # Telemetry facade, TelemetryFormatter
├── BatteryMonitor/       # Coulomb counting, SoC
├── Settings/             # Persistent configuration
├── Button/               # User interface
├── Buzzer/               # Sonorous alerts
└── Xctod/                # BLE telemetry
```

### Adding New Components
1. Create a class in `src/ComponentName/`.
2. Add includes in `config.h`.
3. Create a global object instance.
4. Call its methods in `main.cpp`.

## 🤝 Contributing

Contributions are welcome! Please:

1. Fork the repository.
2. Create a feature branch (`git checkout -b feature/new-feature`).
3. Commit your changes (`git commit -am 'Add new feature'`).
4. Push to the branch (`git push origin feature/new-feature`).
5. Open a Pull Request.

### Code Standards
- Code in English.
- Document new features.
- Follow PlatformIO standards.

## 📄 License

This project is licensed under the MIT License.

---

**Developed by**: Rodrigo Medeiros
**Repository**: [https://github.com/rodrigomedeirosbrazil/fly-controller](https://github.com/rodrigomedeirosbrazil/fly-controller)

*For technical support or questions, open an issue in the repository.*