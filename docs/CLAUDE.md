# docs/ — Directory Guide

This directory contains hardware documentation and user-facing manuals for the Fly Controller project (ESP32-C3 flight controller for electric paramotors).

## Files

| File | Purpose |
|------|---------|
| `MANUAL-DE-USO.md` | End-user operating manual (Portuguese). Covers startup, throttle calibration, arm/disarm procedure, power limiting behavior (battery voltage, motor temp, ESC temp). |
| `MANUAL-INTERFACE-WEB.md` | Web interface manual (Portuguese). Documents all five web pages (Dashboard, Telemetry, Firmware, Logs, Configuration), every configuration field, API endpoints, and URL reference. |
| `device-web-improvements-plan.md` | Internal dev planning doc (English). Describes the phased implementation of the current web UI (telemetry page, page split, dashboard). All phases are marked complete — treat this as historical context, not an active task list. |
| `Schematic_flycontroller_2025-10-29.png` | Circuit schematic for the fly controller PCB (dated 2025-10-29). |
| `PCB_PCB_flycontroller_2025-10-29.png` | PCB layout image (dated 2025-10-29). |
| `Gerber_flycontroller_PCB_flycontroller_2025-10-13.zip` | Gerber files for PCB fabrication (dated 2025-10-13; slightly older than the schematic/layout images). |

## Language conventions

- **User-facing manuals** (`MANUAL-*.md`) are written in **Brazilian Portuguese**. Keep them in Portuguese when editing.
- **Internal planning/dev docs** (`device-web-improvements-plan.md`) are in **English**. New dev docs should also be in English.
- **Code and comments** in the firmware (`src/`) are in English.

## Hardware context (relevant to docs)

- MCU: ESP32-C3 (Lolin C3 Mini)
- Battery: 14S LiPo pack; voltage thresholds configured per-cell in the web UI
- Three supported ESC/motor builds: `lolin_c3_mini_hobbywing`, `lolin_c3_mini_tmotor`, `lolin_c3_mini_xag`
- Optional BMS: JBD or Daly D2 BLE connected over Bluetooth
- Web server runs as a Wi-Fi AP at `192.168.4.1`; key API routes are `GET /api/telemetry`, `GET /config/values`, `POST /config/save`

## Documentation maintenance guidelines

- When firmware adds or renames a configuration field, update the corresponding section in `MANUAL-INTERFACE-WEB.md` (Section 7).
- When the telemetry API (`/api/telemetry`) changes fields or availability flags, update the field table in `MANUAL-INTERFACE-WEB.md` (Section 4) and the technical notes (Section 9).
- When arm/disarm logic or calibration behavior changes, update `MANUAL-DE-USO.md`.
- The Schematic and PCB images are static artifacts tied to a specific board revision. Do not replace them without also updating the Gerber zip and noting the new revision date in filenames.
- `device-web-improvements-plan.md` is a completed plan; do not update its checkboxes. If new web UI work is planned, create a new planning doc.
