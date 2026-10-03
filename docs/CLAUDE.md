# docs/ — Directory Guide

This directory contains hardware documentation and user-facing manuals for the Fly Controller project (ESP32-C3 flight controller for electric paramotors).

## Files

| File | Purpose |
|------|---------|
| `MANUAL-DE-USO.md` | End-user operating manual (Portuguese). Covers startup, throttle calibration, arm/disarm procedure, power limiting behavior (battery voltage, motor temp, ESC temp). |
| `BLE-CONTROL-PROTOCOL.md` | Client-facing reference (English) for the Fly Control GATT service the fly-app talks to — UUIDs, the packed telemetry/INFO/config struct layouts field by field, the CMD/RSP frame format, opcodes, status codes, the auth/armed gate, flight-log access and firmware update. The header it describes is `src/BleControl/ControlProtocol.h`, duplicated by hand as Dart in the fly-app repo; update this file whenever that header changes, and remember the struct layouts are pinned by `test/ControlProtocolTest.cpp` on this side and must be pinned on the Dart side too. |
| `XCTOD-PROTOCOL.md` | Field-by-field reference (English) for the Xctod BLE telemetry sentence and its matching CSV log columns — field order/index, units, validity/blanking rules, and the differences between the two formats. Update whenever `src/Xctod/Xctod.cpp` or `src/TelemetryLogger/TelemetryLogger.cpp` changes field order, count, or meaning; a field-order change there also requires updating `xctrack-pages.xcfg`'s `WExternalData` `index` values. |
| `Schematic_flycontroller_2025-10-29.png` | Circuit schematic for the fly controller PCB (dated 2025-10-29). |
| `PCB_PCB_flycontroller_2025-10-29.png` | PCB layout image (dated 2025-10-29). |
| `Gerber_flycontroller_PCB_flycontroller_2025-10-13.zip` | Gerber files for PCB fabrication (dated 2025-10-13; slightly older than the schematic/layout images). |

## Language conventions

- **User-facing manuals** (`MANUAL-*.md`) are written in **Brazilian Portuguese**. Keep them in Portuguese when editing.
- **Internal planning/dev docs** (`XCTOD-PROTOCOL.md`, `BLE-CONTROL-PROTOCOL.md`) are in **English**. New dev docs should also be in English.
- **Code and comments** in the firmware (`src/`) are in English.

## Hardware context (relevant to docs)

- MCU: ESP32-C3 (Lolin C3 Mini)
- Battery: 14S LiPo pack; voltage thresholds configured from the fly-app
- Two supported ESC/motor builds: `lolin_c3_mini_tmotor`, `lolin_c3_mini_xag`
- Optional BMS: JBD, Daly D2 BLE, or JK (JK02 BLE) connected over Bluetooth
- There is no web interface or WiFi AP: configuration, logs, firmware update and clock sync go through the fly-app over BLE (see `BLE-CONTROL-PROTOCOL.md`). The radio runs WiFi in STA mode only for ESP-NOW to the remote throttle.

## Documentation maintenance guidelines

- When firmware adds or renames a configuration field, or changes the telemetry/INFO structs or opcodes, update `BLE-CONTROL-PROTOCOL.md` (and keep it matching `src/BleControl/ControlProtocol.h`).
- When a user-visible setting or procedure changes, update `MANUAL-DE-USO.md` (section 8 covers configuration through the app).
- When arm/disarm logic or calibration behavior changes, update `MANUAL-DE-USO.md`.
- The Schematic and PCB images are static artifacts tied to a specific board revision. Do not replace them without also updating the Gerber zip and noting the new revision date in filenames.
