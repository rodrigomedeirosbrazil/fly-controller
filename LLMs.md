
#Fly Controller

## Overview

This project, "Fly Controller," is an Arduino-based firmware for a customizable flight controller designed for drones and UAVs. It leverages the PlatformIO build system and is developed for the ATmega328P microcontroller. The controller communicates with other drone components, such as Electronic Speed Controllers (ESCs), using the DroneCAN protocol.

All code must be in English, inclusive comments.

## Key Technologies & Libraries

- **Framework:** Arduino
- **Platform:** Atmel AVR (specifically `pro16MHzatmega328`)
- **Core Libraries:**
  - `AceButton`: For handling button inputs.
  - `autowp-mcp2515`: For CAN bus communication via the MCP2515 controller.
  - `Servo`: For controlling the ESC.
- **Simulation:** The project is configured for simulation in the Wokwi environment, with a `wokwi.toml` file defining the virtual hardware setup.

## Project Structure

The source code is organized in a modular fashion within the `src/` directory:

- `main.cpp`: The main application entry point, containing the `setup()` and `loop()` functions.
- `config.h`: Likely contains compile-time configuration and constants.
- **Components:**
  - `Button/`: Handles user input from a pushbutton.
  - `Buzzer/`: Controls an audible buzzer for signals and warnings.
  - `Canbus/`: Manages DroneCAN communication.
  - `SerialScreen/`: Outputs data to a serial monitor for debugging and status updates.
  - `Temperature/`: Monitors temperature sensors.
  - `Throttle/`: Manages the throttle control logic, including a cruise control feature.

## Core Functionality

- **Throttle Control:** Reads a potentiometer to set the throttle and sends a corresponding PWM signal to the ESC. It includes logic for arming, disarming, and a cruise control feature.
- **CAN Bus Communication:** Interfaces with a CAN bus to send and receive data from other devices, such as an ESC, to get telemetry like voltage, current, and temperature.
- **Safety Features:**
  - Monitors battery voltage and ESC/motor temperature, reducing throttle or disarming the ESC if limits are exceeded.
  - Includes current limiting logic.
- **User Interface:**
  - A single button is used for arming the throttle and activating cruise control.
  - A buzzer provides audible feedback for status changes and warnings.
  - A serial screen provides detailed telemetry and status information.

## Simulation

The `wokwi.toml` file defines a simulation environment that includes:
- A potentiometer for throttle input.
- An NTC temperature sensor.
- LEDs for status indication.
- A buzzer.
- A CAN bus transceiver and message generator for simulating CAN communication.
