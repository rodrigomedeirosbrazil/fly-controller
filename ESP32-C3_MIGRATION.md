# ESP32-C3 Mini Migration Guide

## 🎯 Hardware Configuration

### Microcontroller: ESP32-C3 Mini
- **GPIO Count**: 13 usable pins
- **ADC**: 12-bit (0-4095), 5 channels available
- **Voltage**: 3.3V logic level
- **CAN**: TWAI controller integrated (eliminates MCP2515)

---

## 🔌 Complete Pinout

### Connection Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                      ESP32-C3 MINI                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  3V3           ─────► Power (Hall, NTC, SN65HVD230)        │
│  GND           ─────► Ground (All components)               │
│                                                             │
│  GPIO0 (ADC1-0) ────► Hall Sensor Signal                   │
│  GPIO1 (ADC1-1) ────► NTC 10K Signal                       │
│  GPIO2          ────► SN65HVD230 CTX (TXD)                 │
│  GPIO3          ────► SN65HVD230 CRX (RXD)                 │
│  GPIO5          ────► Push Button (internal pull-up)       │
│  GPIO6          ────► Buzzer (active/passive compatible)    │
│  GPIO7          ────► ESC PWM Signal                        │
│                                                             │
│  GPIO20 (RX)    ────► USB Serial (automatic)               │
│  GPIO21 (TX)    ────► USB Serial (automatic)               │
│                                                             │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                   SN65HVD230 Transceiver                    │
├─────────────────────────────────────────────────────────────┤
│  3.3V    ◄───── ESP32-C3 3V3                               │
│  GND     ◄───── ESP32-C3 GND                               │
│  CTX     ◄───── ESP32-C3 GPIO2 (TWAI_TX)                   │
│  CRX     ─────► ESP32-C3 GPIO3 (TWAI_RX)                   │
│  CANH    ─────► CAN Bus High (to Hobbywing + JK-BMS)      │
│  CANL    ─────► CAN Bus Low  (to Hobbywing + JK-BMS)      │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                    Component Connections                    │
├─────────────────────────────────────────────────────────────┤
│  Hall Sensor:                                               │
│    VCC ◄───── 3.3V                                         │
│    GND ◄───── GND                                          │
│    OUT ─────► GPIO0                                        │
│                                                             │
│  NTC 10K (Temperature):                                     │
│    One side ◄───── 3.3V                                    │
│    Middle   ─────► GPIO1                                   │
│    Other    ◄───── GND via 10K resistor (voltage divider)  │
│                                                             │
│  Push Button:                                               │
│    One side ◄───── GPIO5 (internal pull-up enabled)        │
│    Other    ◄───── GND                                     │
│                                                             │
│  Buzzer (Active/Passive):                                   │
│    +  ◄───── GPIO6                                         │
│    -  ◄───── GND                                           │
│                                                             │
│  ESC (Hobbywing):                                           │
│    Signal ◄───── GPIO7 (PWM 1050-1950μs)                   │
│    +      ◄───── Battery voltage                           │
│    -      ◄───── GND                                       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 📊 Pin Mapping Table

| Component | ESP32-C3 Pin | Type | Voltage | Notes |
|-----------|--------------|------|---------|-------|
| **Hall Sensor (Throttle)** | GPIO0 (ADC1-0) | ADC Input | 0-3.3V | Auto-calibration |
| **NTC 10K (Motor Temp)** | GPIO1 (ADC1-1) | ADC Input | 0-3.3V | Voltage divider |
| **CAN TX to SN65HVD230** | GPIO2 | TWAI TX | 3.3V | Connect to CTX |
| **CAN RX from SN65HVD230** | GPIO3 | TWAI RX | 3.3V | Connect to CRX |
| **Push Button** | GPIO5 | Digital IN | 3.3V | Pull-up enabled |
| **Buzzer** | GPIO6 | Digital/PWM | 3.3V | Active/passive compatible |
| **ESC PWM** | GPIO7 | PWM OUT | 3.3V | 1050-1950μs |
| **USB Serial RX** | GPIO20 | UART | 3.3V | Automatic |
| **USB Serial TX** | GPIO21 | UART | 3.3V | Automatic |

**Free Pins for Expansion:** GPIO4, GPIO8, GPIO9, GPIO10 (4 pins + SPI)

---

## ⚡ Key Differences from Arduino

| Feature | Arduino ATmega328P | ESP32-C3 | Impact |
|---------|-------------------|----------|--------|
| **Logic Level** | 5V | 3.3V | ⚠️ All I/O must be 3.3V |
| **ADC Resolution** | 10-bit (0-1023) | 12-bit (0-4095) | ✅ Better precision |
| **ADC Reference** | 5V | 3.3V | ⚠️ Code updated |
| **CAN Controller** | External MCP2515 | Integrated TWAI | ✅ Simpler, faster |
| **SPI for CAN** | 5 pins | 0 pins | ✅ Saves 5 GPIO |
| **WiFi/Bluetooth** | None | Built-in | ✅ Telemetry ready |
| **Flash** | 32KB | 4MB | ✅ More space |
| **RAM** | 2KB | 400KB | ✅ Much more memory |

---

## 🔧 Software Changes Made

### Files Modified:

1. **platformio.ini** - Changed to ESP32-C3 platform
2. **config.h** - Updated pin definitions and TWAI config
3. **config.cpp** - Removed MCP2515 instance
4. **main.cpp** - TWAI initialization instead of MCP2515
5. **Temperature.cpp** - Updated for 3.3V and 12-bit ADC
6. **Throttle.cpp** - Updated ADC max value to 4095
7. **Canbus.h/cpp** - Changed from `can_frame` to `twai_message_t`
8. **Hobbywing.h/cpp** - Updated for TWAI message structure
9. **Jkbms.h/cpp** - Updated for TWAI message structure

### Key Code Changes:

**ADC Configuration:**
```cpp
// Added in setup()
analogSetAttenuation(ADC_11db);  // Full 0-3.3V range
```

**TWAI Initialization:**
```cpp
twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
  (gpio_num_t)CAN_TX_PIN, 
  (gpio_num_t)CAN_RX_PIN, 
  TWAI_MODE_NORMAL
);
twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

twai_driver_install(&g_config, &t_config, &f_config);
twai_start();
```

**Message Structure:**
```cpp
// Old (MCP2515):
struct can_frame canMsg;
canMsg.can_id = id | CAN_EFF_FLAG;
canMsg.can_dlc = 8;
mcp2515.sendMessage(&canMsg);

// New (TWAI):
twai_message_t canMsg;
canMsg.identifier = id;
canMsg.extd = 1;  // Extended 29-bit
canMsg.data_length_code = 8;
twai_transmit(&canMsg, pdMS_TO_TICKS(10));
```

---

## 🛠️ Build and Upload

```bash
# Clean previous build
pio run --target clean

# Build for ESP32-C3
pio run

# Upload to ESP32-C3
pio run --target upload

# Monitor serial output
pio device monitor
```

---

## ✅ Testing Checklist

- [ ] 3.3V power supply to Hall sensor
- [ ] 3.3V power supply to NTC
- [ ] 3.3V power supply to SN65HVD230
- [ ] Hall sensor calibration working
- [ ] Temperature reading accurate
- [ ] Button press detected
- [ ] Buzzer sounds
- [ ] ESC PWM signal working
- [ ] TWAI driver starts successfully
- [ ] CAN messages received from Hobbywing ESC
- [ ] CAN messages received from JK-BMS
- [ ] Throttle control functioning
- [ ] Safety protections active

---

## 🚨 Important Notes

1. **Never apply 5V to ESP32-C3 GPIO pins** - Will damage the chip!
2. **Hall sensor must be 3.3V powered** - Output will be 0-3.3V
3. **SN65HVD230 must be 3.3V version** - Check your module
4. **CAN bus needs 120Ω termination** - At both ends of the bus
5. **TWAI uses GPIO2/3** - These are ADC2 pins, don't use WiFi simultaneously with ADC2
6. **Auto-calibration will adjust to new ADC range** - No manual calibration needed

---

## 📈 Expansion Possibilities

With 4 free GPIO pins, you can add:

- **OLED Display** (I2C on GPIO8/9)
- **SD Card** (SPI on GPIO8/9/10)
- **Extra Sensors** (GPIO4 as ADC or digital)
- **WiFi Telemetry** (built-in, just enable)
- **Bluetooth** (built-in, just enable)
- **OTA Updates** (WiFi)

---

## 📝 Wiring Summary

**Power Distribution:**
```
Battery (+) ──► ESC Power
Battery (-) ──► Common GND
ESP32-C3 USB ──► 5V Power (USB powers ESP32 internally to 3.3V)

ESP32 3V3 ──┬──► Hall Sensor VCC
            ├──► NTC Top
            ├──► SN65HVD230 3.3V
            └──► (other 3.3V components)

Common GND ──┬──► Hall Sensor GND
             ├──► NTC Bottom (via 10K)
             ├──► SN65HVD230 GND
             ├──► Button
             ├──► Buzzer (-)
             └──► ESC Signal GND
```

**CAN Bus:**
```
SN65HVD230 CANH ───┬──► Hobbywing ESC CANH
                   └──► JK-BMS CANH

SN65HVD230 CANL ───┬──► Hobbywing ESC CANL
                   └──► JK-BMS CANL

Note: Add 120Ω termination resistor between CANH-CANL at bus ends
```

---

## 🔍 Troubleshooting

**TWAI driver won't start:**
- Check GPIO2/3 connections to SN65HVD230
- Verify SN65HVD230 is powered (3.3V)
- Check TX goes to CTX, RX goes to CRX

**No CAN messages:**
- Verify 120Ω termination on CAN bus
- Check CANH/CANL connections
- Confirm ESC and BMS are powered and transmitting

**ADC readings wrong:**
- Verify analogSetAttenuation() is called in setup()
- Check voltage divider for NTC (should be 10K)
- Confirm Hall sensor is powered with 3.3V

**Hall sensor calibration fails:**
- Ensure sensor outputs 0-3.3V range
- Check ADC_MAX_VALUE is set to 4095
- Try manual calibration if auto fails

---

## 📞 Support

For issues or questions:
1. Check serial monitor output for TWAI status
2. Verify all connections match this diagram
3. Test each component individually
4. Open issue on GitHub repository

---

**Migration Date:** October 2025  
**Target Board:** ESP32-C3 Mini DevKitM-1  
**Firmware Version:** Compatible with existing fly-controller codebase
