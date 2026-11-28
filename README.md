# Fly Controller

**An intelligent and modular flight controller for drones and UAVs**

Fly Controller is an advanced firmware that implements a complete flight control system with CAN bus communication and intelligent power management. Designed to work with T-Motor (UAVCAN) ESCs, Hobbywing (PWM throttle, DroneCAN telemetry) ESCs and XAG (PWM-only) motors on the ESP32-C3 platform.

**Pilots:** the user manual, in Portuguese, is at [docs/manual](docs/manual/README.md).

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

- **Intelligent Throttle Control**: With automatic calibration, an engage gate against idle drift, and safety protections.
- **CAN Bus Communication**: Complete UAVCAN interface with T-Motor ESCs.
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
- **CAN Bus**: Built-in ESP32 TWAI controller for UAVCAN (1 Mbps, T-Motor). The Hobbywing build uses it at 500 kbps for receive-only DroneCAN telemetry.
- **Bluetooth LE**: For telemetry transmission.
- **Serial**: Telemetry and debug (115200 baud).
- **PWM**: Direct ESC control.

### Analog Inputs (via ADS1115 I2C 16-bit ADC)
- **Hall Sensor**: Throttle control (ADS1115 Channel 0)
- **NTC Sensor**: Motor temperature (ADS1115 Channel 1)
- **NTC Sensor** *(XAG only)*: ESC temperature (ADS1115 Channel 2)
- **Voltage Divider** *(all builds)*: Battery voltage (ADS1115 Channel 3)

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
- Engage gate (hysteresis) so idle drift never spins the motor
- Noise filtering with moving average
- Sensor failure protection
```

**Technical Specifications:**
- Read every 10 ms through the ADS1115; 8-sample moving average (80 ms window).
- Calibrated on every boot: hold full throttle for 3 s, then rest for 3 s. Not saved.
- The motor only engages above 2% of the calibrated range and releases below 1%.
- Pilot demand (0-100%) is a continuous rescale from the engage point to the top; the top 3% of travel reads 100%. The same value feeds the PWM, the log, `$XCTOD`, BLE telemetry and the arming check (arming is refused while the gate is engaged).
- Calibration accepts a stable 3 s hold as the maximum only if it is at least 600 counts above the lowest reading seen since power-on (an untouched throttle stays silent; a throttle held open at power-on must be released and squeezed again), then a stable hold at least 600 counts below it as the minimum. A partial release restarts the calibration with a warning beep.
- The motor command goes through the throttle output stage (see section 6): soft start, instant response while running, bounded fall rate.
- Any invalid reading (outside the calibrated band, or repeated I2C failures) disarms at once.

### 2. **TmotorCan** - UAVCAN Interface
```cpp
// TM-UAVCAN v2.3 protocol for T-Motor ESCs
- Complete telemetry (RPM, voltage, current, temperature)
- RawCommand throttle sent at 400 Hz
- Multi-frame transfer reassembly
```

**Supported Messages:**
- ESC_STATUS (1034): RPM, voltage, current
- Status 5 (1154): motor temperature (the only CAN source of it; EMA-filtered, tau 2 s) — only sent when the ESC's "Status protocol" is set to `DRONECAN-S` (CloudLink); `CUBECAN` mode does not send it. A protocol change takes effect only after the ESC itself is power-cycled.
- RawCommand (1030): throttle command

*XAG uses XagTelemetry with analog sensors (PWM-only, no CAN).*

*Hobbywing uses HobbywingCan (receive-only) for current, RPM and ESC temperature, and HobbywingTelemetry for the rest; the throttle goes to the ESC by PWM.*

### 3. **Power** - Power Management
```cpp
// Intelligent available power calculation
- Battery voltage-based limitation
- Motor thermal protection
- Battery power floor
- ESC PWM conversion (throttle position caps at the allowed ceiling)
```

**Protection Algorithms:**
- Minimum voltage limitation (42.0V)
- Thermal protection (60°C motor, 110°C ESC)
- Progressive power reduction based on motor and ESC temperature
- The limits act as a ceiling on the throttle output stage's target; only the downward fall rate is limited (no acceleration ramp)

### 4. **Temperature** - Thermal Monitoring
```cpp
// Agnostic NTC sensor (ReadFn + adcVoltageRef)
- Beta = 3950K for 10K NTC
- Filtering with 10 samples
- ADC source: ADS1115 I2C (all builds — legacy GPIO ADC not used in production)
```

### 5. **Canbus** - Communication Management
```cpp
// CAN message handling using ESP32 TWAI controller
- receive() non-blocking API - returns raw ESC frames
- main.cpp routes frames to TmotorCan (Tmotor) or HobbywingCan (Hobbywing)
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
- Arm: short click, release, then hold for 2 s (throttle at rest)
- Disarm: hold; power ramps down over 2 s (with the throttle at rest it disarms at once and any residual output falls over ~1 s)
- Audio feedback with the buzzer
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

### 9. **BleControl** - Fly Control Service (fly-app)
```cpp
// Second GATT service on the shared BLE server, used by the fly-app
- Live telemetry notifications at 1 Hz
- Configuration groups: power, thermal, BMS, system (PIN-protected writes)
- Firmware over-the-air (OTA) updates over BLE
- CSV flight log listing, download and deletion
```
See docs/BLE-CONTROL-PROTOCOL.md for the wire contract.

### 10. **BMS Integration** - Battery Management Systems
```cpp
// BLE client for smart battery pack monitoring (Daly, JBD, JK and ANT/JNFN)
- Cell voltage monitoring (min, max, delta)
- Pack voltage, current, and state-of-charge (SoC)
- Temperature sensors per pack
- Charge/discharge/balance status flags
- BLE device scanning triggered from the fly-app
```

### 11. **Logger** - Flight Data Recording
```cpp
// CSV logging to LittleFS
- Auto-start on arm, auto-stop on disarm
- Configurable data fields and header
- Log download and deletion from the fly-app
```

## 📡 Communication Protocols

### CAN Bus Protocols

#### UAVCAN (T-Motor ESC)
- **Speed**: 1 Mbps
- **Protocol**: UAVCAN v0
- **Messages**: Status, control, configuration
- **Format**: CAN 2.0B with UAVCAN payload
- **Telemetry**: RPM, voltage, current, temperature

### Bluetooth LE (XCTOD)
- Provides a service to transmit flight data to compatible mobile applications (XCTRACK).
- Available in all controller modes.
- Field-by-field protocol reference: [docs/XCTOD-PROTOCOL.md](docs/XCTOD-PROTOCOL.md).

## ⚡ Features

### Safety
- ✅ Low voltage protection
- ✅ Motor thermal limitation
- ✅ ESC thermal limitation
- ✅ Auto-disarm on failure
- ✅ Sonorous alert when motor stops

### Control
- ✅ Automatic throttle calibration
- ✅ Button arming/disarming
- ✅ Direct ESC PWM control
- ✅ Throttle engage gate against idle drift
- ✅ Bidirectional CAN bus communication (UAVCAN)

### Monitoring
- ✅ Bluetooth LE telemetry for apps (XCTRACK)
- ✅ Complete serial telemetry
- ✅ Real-time status
- ✅ Event logging
- ✅ Failure diagnosis
- ✅ Battery Management System support (Daly, JBD, JK, ANT/JNFN via BLE)
- ✅ Cell voltage and balance monitoring
- ✅ State-of-charge (SoC) tracking
- ✅ CSV flight logging to LittleFS (auto on arm/disarm)
- ✅ Fly Control BLE service (fly-app) with OTA firmware updates

## 🔌 Supported Hardware

### Microcontroller
- **ESP32-C3 Super Mini**

### ESC
- **T-Motor** (UAVCAN mode - default): Full telemetry via CAN bus
- **Hobbywing** (Hobbywing mode): PWM throttle, with current, RPM and ESC temperature read from DroneCAN status frames (receive-only, 500 kbps)
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

The project supports three controller types.

| Firmware | Environment | Protocol | CAN Bus |
|------------|-------------|----------|---------|
| **T-Motor** (default) | `lolin_c3_mini_tmotor` | UAVCAN | ✅ Required |
| **XAG** | `lolin_c3_mini_xag` | PWM-only | ❌ Not required |
| **Hobbywing** | `lolin_c3_mini_hobbywing` | PWM throttle, DroneCAN telemetry | ✅ Required (500 kbps, receive-only) |

**Quick Build Commands:**
```bash
# Controller targets
~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor
~/.platformio/penv/bin/pio run -e lolin_c3_mini_xag
~/.platformio/penv/bin/pio run -e lolin_c3_mini_hobbywing
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
    bxparks/AceButton@^1.10.1
    madhephaestus/ESP32Servo
    adafruit/Adafruit ADS1X15@^2.4.6
```

### 4. Build and Upload

**Using VS Code PlatformIO Extension (Recommended):**
- Install the PlatformIO extension in VS Code
- Open the project folder
- Select the desired environment from the PlatformIO toolbar (bottom status bar)
- Use the PlatformIO toolbar to build, upload, and monitor

**Using PlatformIO CLI:**

The project supports three controller types, each with its own build environment:

#### T-Motor Controller (Default - UAVCAN)
```bash
# Build for T-Motor ESC (UAVCAN protocol)
~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor

# Or use the default environment
~/.platformio/penv/bin/pio run -e lolin_c3_mini

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

#### Hobbywing Controller (PWM throttle, DroneCAN telemetry)
```bash
# Build for Hobbywing ESCs (PWM throttle, DroneCAN telemetry at 500 kbps)
~/.platformio/penv/bin/pio run -e lolin_c3_mini_hobbywing

# Upload firmware
~/.platformio/penv/bin/pio run -e lolin_c3_mini_hobbywing -t upload

# Monitor serial output
~/.platformio/penv/bin/pio device monitor -e lolin_c3_mini_hobbywing
```

**Features:**
- Throttle to the ESC by PWM only; the firmware sends no throttle or configuration commands over CAN, only the generic NodeStatus / GetNodeInfo frames
- Current, RPM and ESC temperature read from the ESC's DroneCAN status frames (receive-only)
- Battery voltage from the voltage divider and motor temperature from the NTC (not from CAN)

**Note:** Requires the board with the battery voltage divider; older boards without it are not supported.

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

**Note:** The XAG motor has a ~1.5 s reaction delay when stopped. The throttle output stage holds 5% PWM for that wake-up, then ramps to 20% over ~3 s before releasing to full response (see section 6).

### 5. Pin Configuration
```cpp
// src/config.h - Main configurations

// I2C Bus (ADS1115 ADC - all builds)
#define I2C_SDA_PIN 20  // GPIO20 - I2C SDA
#define I2C_SCL_PIN 21  // GPIO21 - I2C SCL

// ADS1115 Channels
#define ADS1115_THROTTLE_CHANNEL   0  // Channel A0 - Hall Sensor
#define ADS1115_MOTOR_TEMP_CHANNEL 1  // Channel A1 - Motor NTC 10K
#define ADS1115_ESC_TEMP_CHANNEL   2  // Channel A2 - ESC NTC 10K (XAG only)
#define ADS1115_BATTERY_CHANNEL    3  // Channel A3 - Battery voltage divider (XAG, Tmotor, Hobbywing)

// Digital I/O
#define BUTTON_PIN 5  // GPIO5 - Push button
#define BUZZER_PIN 6  // GPIO6 - Passive Buzzer (PWM)
#define ESC_PIN    7  // GPIO7 - ESC PWM signal

// CAN Bus (TWAI) - T-Motor mode only
#define CAN_TX_PIN 2  // GPIO2 - CAN TX
#define CAN_RX_PIN 3  // GPIO3 - CAN RX
```

### 6. Throttle Output Stage
`src/ThrottleOutput/ThrottleOutputLogic.h` (pure, host-tested in `test/ThrottleOutputLogicTest.cpp`) turns the pilot demand into the ESC command. Per-board parameters live in `BoardConfig` (`ThrottleOutputConfig`); all values are provisional until the bench measurements.

**States:**
- **Stopped**: output at minimum. When the engage gate opens (armed) it goes to Starting, or straight to Running on a board with the soft start off.
- **Starting**: the output rises at most at the start ramp rate from its current value, up to the ramp top (XAG: held at the wake level during the wake-up wait first). No running offset. It releases to Running by fresh RPM at or above the release RPM (T-Motor), or after the time fallback once the output has reached the ramp top. If the gate releases it returns to Stopped.
- **Running**: instant response to increases, with the running offset (`offset + (1 - offset) * demand`). It returns to Stopped only with the output at minimum and the motor confirmed stopped (fresh RPM = 0 for 500 ms, or the coast time when RPM is stale or absent). RPM = 0 while the motor is commanded never counts as stopped.
- **Any disarm** (button, fault, battery lost): the target is minimum and the decel limiter brings the output down from the last value; the stage keeps running until it reaches minimum. A fault disarm never follows the faulty reading.

Order of operations: demand, running offset, start limit, limiter ceiling and button disarm scale, decel limiter (downward only), PWM.

| | T-Motor | XAG | Hobbywing |
|---|---|---|---|
| Soft start | yes | yes | no |
| Wake-up wait | 0 | 1500 ms at 5% | - |
| Start ramp | to 20%, ~3 s | 5% to 20%, ~2 s | - |
| Release | RPM >= 600, or 4 s once at the ramp top | output at the ramp top | - |
| Running offset | 0 | 0 | 0 |
| Decel (full to min) | ~1 s | ~1 s | ~1 s |
| Stop detection | RPM = 0 for 500 ms (coast 3 s if stale) | coast 800 ms | RPM = 0 for 500 ms (coast 3 s if stale) |

`isMotorRunning()` is armed and the stage output above minimum, so it includes the start and decel ramps.

### 7. Calibration
The system automatically calibrates the Hall sensor for the throttle on every startup. No manual configuration is needed.

**Calibration Process:**
1. After power-on, with the throttle at rest, move it to maximum and hold for 3 seconds; two short beeps mean the maximum was captured: release
2. Move throttle to minimum position and hold for 3 seconds (at least 600 counts below the maximum)
3. Calibration complete - system is ready to arm

Nothing sounds while the throttle stays untouched. If it was held open at power-on, release it and squeeze again. A partial release after the maximum restarts the calibration with a warning beep.

## 📊 Usage Example

### Operation Sequence
1. **Initialization**: Automatic throttle calibration, CAN bus setup (if applicable).
2. **Arming**: Click, release, then hold the button for 2 s (throttle must be at rest).
3. **Operation**: Adjust the throttle; start from stopped is soft, then the system responds instantly, capped by the battery and temperature limits. Releases fall over about 1 s.
4. **Disarm**: Hold the button; power ramps down over 2 s and the system disarms.

### Controller-Specific Notes

**T-Motor (CAN bus):**
- Ensure CAN bus is properly connected (H/L lines)
- ESC must be configured for UAVCAN mode
- Full telemetry available via CAN bus

**Hobbywing (PWM throttle, DroneCAN telemetry):**
- Connect the CAN bus (H/L lines) at 500 kbps; telemetry is receive-only
- Throttle is PWM on GPIO7, as on XAG
- Motor temperature sensor (NTC) and battery voltage divider must be connected
- Needs the board with the battery voltage divider (older boards without it are not supported)

**XAG (PWM-only):**
- No CAN bus required
- Temperature sensors must be connected (motor and ESC)
- Battery voltage divider must be connected
- Motor has ~1.5 s reaction delay when stopped; firmware holds 5% PWM during wake-up, then ramps to 20% over ~3 s (throttle output stage)

## 🛠️ Development

### File Structure
```
src/
├── main.cpp              # Main loop
├── config.h, config.cpp  # Configuration and object instances
├── config_controller.h   # Build type (IS_TMOTOR, IS_XAG, IS_HOBBYWING)
├── ADS1115/              # I2C 16-bit ADC (primary sensor interface, all builds)
├── Throttle/             # Throttle control (ReadFn via ADS1115)
├── Temperature/          # Thermal sensor (ReadFn via ADS1115)
├── Power/                # Power management & limiter ceilings
├── Canbus/               # CAN receive() API (Tmotor, Hobbywing)
├── Tmotor/               # TmotorCan, TmotorTelemetry
├── Hobbywing/            # HobbywingCan (receive-only), HobbywingTelemetry, HobbywingCanParser
├── Xag/                  # XagTelemetry (PWM-only)
├── Sensors/              # BatteryVoltageSensor (XAG)
├── Telemetry/            # Telemetry facade, TelemetryFormatter
├── BatteryMonitor/       # Coulomb counting, SoC
├── Settings/             # Persistent configuration (ESP32 Preferences)
├── Button/               # User interface (AceButton)
├── Buzzer/               # Audible alerts (PWM)
├── Xctod/                # BLE telemetry (XCTRACK format)
├── BluetoothBms/         # BLE BMS facade (auto-detects Daly/JBD/JK/ANT)
├── DalyBms/              # Daly BMS protocol implementation
├── JbdBms/               # JBD BMS protocol implementation
├── JkBms/                # JK BMS (JK02) protocol implementation + parser
├── Logger/               # CSV flight logging to LittleFS
└── BleControl/           # Fly Control GATT service (fly-app)
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