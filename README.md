# Fly Controller

**An intelligent and modular flight controller for drones and UAVs**

Fly Controller is an advanced Arduino firmware that implements a complete flight control system with DroneCAN communication, intelligent power management, and battery monitoring. Specifically designed to work with Hobbywing ESCs and JK-BMS battery systems.

## 📋 Table of Contents

- [Overview](#overview)
- [System Architecture](#system-architecture)
- [Main Components](#main-components)
- [Communication Protocols](#communication-protocols)
- [Features](#features)
- [Supported Hardware](#supported-hardware)
- [Installation and Configuration](#installation-and-configuration)
- [Simulation](#simulation)
- [Contributing](#contributing)
- [License](#license)

## 🚁 Overview

Fly Controller is a modular Arduino-based flight control system that offers:

- **Intelligent Throttle Control**: With automatic calibration, cruise control, and safety protections
- **DroneCAN Communication**: Complete interface with Hobbywing X13 ESCs
- **Battery Monitoring**: Integration with JK-BMS via CAN bus
- **Power Management**: Dynamic power calculation based on voltage and temperature
- **User Interface**: Control button and buzzer for feedback
- **Complete Telemetry**: Serial output for real-time monitoring

## 🏗️ System Architecture

### Microcontroller
- **Platform**: ATmega328P (16MHz)
- **Framework**: Arduino
- **Build System**: PlatformIO

### Communication
- **CAN Bus**: MCP2515 controller for DroneCAN and JK-BMS
- **Serial**: Telemetry and debug (9600 baud)
- **PWM**: Direct ESC control

### Analog Inputs
- **Hall Sensor**: Throttle control (A1)
- **NTC Sensor**: Motor temperature (A0)

### Digital Outputs
- **ESC PWM**: Pin 9 (1050-1950μs)
- **Buzzer**: Pin 4
- **Button**: Pin 3 (with interrupt)

## 🔧 Main Components

### 1. **Throttle** - Throttle Control
```cpp
// Main features
- Automatic Hall sensor calibration
- System arming/disarming
- Cruise control mode
- Noise filtering with moving average
- Sensor failure protection
```

**Technical Specifications:**
- Hall sensor reading range: 170-850 (analog)
- 30 samples for filtering
- Automatic calibration in 3 seconds
- Cruise mode activated with 30%+ throttle

### 2. **Hobbywing** - DroneCAN Interface
```cpp
// DroneCAN protocol for Hobbywing X13 ESC
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

### 3. **Jkbms** - Battery Monitoring
```cpp
// JK-BMS battery management system
- Individual cell monitoring
- State of Charge (SOC) and Health (SOH)
- Safety protections
- Battery temperature
```

**Monitored Data:**
- Total and per-cell voltage (up to 24 cells)
- Charge/discharge current
- Temperature (2 sensors)
- SOC/SOH and cycle count
- Protection flags

### 4. **Power** - Power Management
```cpp
// Intelligent available power calculation
- Battery voltage-based limitation
- Motor thermal protection
- Battery power floor
- ESC PWM conversion
```

**Protection Algorithms:**
- Minimum voltage limitation (46.2V)
- Thermal protection (60°C motor)
- Continuous current limitation (105A)
- Progressive power floor

### 5. **Temperature** - Thermal Monitoring
```cpp
// NTC sensor with temperature compensation
- Beta = 3600K for 10K NTC
- Filtering with 10 samples
- Ambient temperature compensation
```

### 6. **Canbus** - Communication Management
```cpp
// Universal parser for CAN messages
- Automatic device detection
- Routing to specific components
- Debug and message logging
```

### 7. **Buzzer** - Audio Interface
```cpp
// Sonorous alert system
- Success, error, and warning beeps
- Customizable beeps
- Continuous alert when motor stops
```

### 8. **Button** - User Interface
```cpp
// Single button control
- Single click: Arming/Disarming
- Long press: Cruise activation
- Tactile feedback with buzzer
```

### 9. **Xctod** - Serial Telemetry
```cpp
// Data output for monitoring
- Battery information
- Throttle status
- Motor and ESC data
- Overall system status
```

## 📡 Communication Protocols

### DroneCAN (Hobbywing ESC)
- **Speed**: 500 kbps
- **Node ID**: 0x13 (controller), 0x03 (ESC)
- **Messages**: Status, control, configuration
- **Format**: CAN 2.0B with DroneCAN payload

### JK-BMS CAN
- **Speed**: 500 kbps
- **IDs**: 0x100-0x103 (different data types)
- **Protocol**: JK-BMS v2.0
- **Data**: Voltage, current, temperature, protections

## ⚡ Features

### Safety
- ✅ Low voltage protection
- ✅ Motor thermal limitation
- ✅ Current protection
- ✅ Auto-disarm on failure
- ✅ Sonorous alert when motor stops

### Control
- ✅ Automatic throttle calibration
- ✅ Intelligent cruise control
- ✅ Button arming/disarming
- ✅ Direct ESC PWM control
- ✅ Bidirectional DroneCAN communication

### Monitoring
- ✅ Complete serial telemetry
- ✅ Real-time status
- ✅ Event logging
- ✅ Failure diagnosis

## 🔌 Supported Hardware

### Microcontroller
- Arduino Nano/Pro Mini (ATmega328P 16MHz)

### ESC
- Hobbywing X13 (DroneCAN)
- Any ESC with PWM input

### Battery
- JK-BMS with CAN interface
- Li-ion/Li-Po packs (up to 24 cells)

### Sensors
- Hall Sensor (throttle)
- 10K NTC 3950K (temperature)
- MCP2515 (CAN controller)

### Interface
- Push-button
- Piezoelectric buzzer
- OLED display (optional)

## 🚀 Installation and Configuration

### 1. Prerequisites
```bash
# PlatformIO CLI
pip install platformio

# Or use PlatformIO IDE
```

### 2. Clone Repository
```bash
git clone https://github.com/rodrigomedeirosbrazil/fly-controller.git
cd fly-controller
```

### 3. Dependencies
```ini
# platformio.ini already configured with:
lib_deps =
    arduino-libraries/Servo@^1.2.2
    autowp/autowp-mcp2515@^1.2.1
    bxparks/AceButton@^1.10.1
```

### 4. Build and Upload
```bash
# Build
pio run

# Upload to microcontroller
pio run --target upload

# Serial monitor
pio device monitor
```

### 5. Pin Configuration
```cpp
// config.h - Main configurations
#define THROTTLE_PIN A1           // Hall Sensor
#define MOTOR_TEMPERATURE_PIN A0  // NTC
#define BUTTON_PIN 3              // Button
#define BUZZER_PIN 4              // Buzzer
#define ESC_PIN 9                 // ESC PWM
#define CANBUS_CS_PIN 10          // CAN SPI CS
```

### 6. Calibration
1. **Automatic**: System calibrates Hall sensor automatically on initialization
2. **Manual**: Adjust `THROTTLE_PIN_MIN/MAX` in `config.h` if needed

## 🎮 Simulation

The project includes configuration for Wokwi simulation:

```toml
# wokwi.toml
[wokwi]
version = 1
firmware = '.pio/build/pro16MHzatmega328/firmware.hex'
elf = '.pio/build/pro16MHzatmega328/firmware.elf'
```

**Simulation Components:**
- Arduino Nano
- Hall sensor for throttle
- NTC for temperature
- Status LEDs
- Buzzer
- CAN bus transceiver
- OLED display

## 📊 Usage Example

### Serial Telemetry
```
=== FLY CONTROLLER STATUS ===
Battery: 52.3V (SOC: 85%)
Current: -15.2A (discharging)
Temperature: 32°C
Throttle: 45% (Armed)
ESC RPM: 1250
Motor Temp: 28°C
Power: 780W
Cruise: OFF
```

### Operation Sequence
1. **Initialization**: Automatic calibration, CAN bus setup
2. **Arming**: Press button to arm system
3. **Operation**: Adjust throttle, system calculates safe power
4. **Cruise**: Maintain throttle >30% for 30s to activate
5. **Disarm**: Press button again

## 🛠️ Development

### File Structure
```
src/
├── main.cpp              # Main loop
├── config.h              # Configurations
├── Throttle/             # Throttle control
├── Hobbywing/            # DroneCAN interface
├── Jkbms/                # Battery monitoring
├── Power/                # Power management
├── Temperature/          # Thermal sensor
├── Canbus/               # CAN communication
├── Button/               # User interface
├── Buzzer/               # Sonorous alerts
└── Xctod/                # Serial telemetry
```

### Adding New Components
1. Create class in `src/ComponentName/`
2. Add includes in `config.h`
3. Create global object instance
4. Call methods in `main.cpp`

## 🤝 Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/new-feature`)
3. Commit your changes (`git commit -am 'Add new feature'`)
4. Push to the branch (`git push origin feature/new-feature`)
5. Open a Pull Request

### Code Standards
- Code in English (inclusive comments)
- Complete documentation in English
- Unit tests when applicable
- Follow Arduino/PlatformIO standards

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

**Developed by**: Rodrigo Medeiros
**Repository**: [https://github.com/rodrigomedeirosbrazil/fly-controller](https://github.com/rodrigomedeirosbrazil/fly-controller)

*For technical support or questions, open an issue in the repository.*