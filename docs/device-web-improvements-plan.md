# Device Web Pages Improvement Plan

## Goal
Plan and implement two improvements in the device web UI:
1. Add a dedicated telemetry page with live data.
2. Reorganize the current pages, separating concerns currently mixed in the main page.
3. Keep UI lightweight, clean, and visually organized for mobile and desktop.

This plan is based on the current implementation in:
- `src/WebServer/ControllerWebServer.cpp`
- `src/Xctod/Xctod.cpp`
- `src/Logger/Logger.cpp`

## Current State (Today)
- Main page (`/`) includes:
  - Firmware upload form (`/update`)
  - Logs table (`/list`, `/delete`, static download via `/logs`)
  - Link to configuration (`/config`)
- Configuration page exists and is independent (`/config`).
- No dedicated telemetry HTTP page yet.
- Telemetry data already exists in firmware and is sent by BLE/logging through `Xctod`.

## Task 1: Telemetry Page

### 1.1 Scope
Create a new page `GET /telemetry` to display live telemetry using HTTP polling from a new API endpoint.

### 1.2 Data Source (same base as Xctod/log)
Use the same logical fields used in `Xctod::write()` and logger header:
- `battery_percent_cc`
- `battery_percent_voltage`
- `voltage`
- `power_kw`
- `throttle_percent`
- `throttle_raw`
- `power_percent`
- `motor_temp`
- `rpm`
- `esc_current`
- `esc_temp`
- `armed`

### 1.3 New endpoint proposal
Add `GET /api/telemetry` returning JSON.

Suggested JSON contract:
```json
{
  "hasTelemetry": true,
  "batteryPercentCc": 87,
  "batteryPercentVoltage": 84,
  "batteryVoltageMv": 52100,
  "powerKwX10": 7,
  "throttlePercent": 42,
  "throttleRaw": 18400,
  "powerPercent": 78,
  "motorTempMc": 55000,
  "rpm": 3100,
  "escCurrentMa": 14500,
  "escTempMc": 63000,
  "armed": true,
  "uptimeMs": 1234567,
  "lastTelemetryUpdateMs": 1234500
}
```

Notes:
- Keep transport values in integer base units (`mV`, `mA`, `mC`) and format on frontend.
- For controllers without current sensor (`hasCurrentSensor == false`), return `rpm`/`escCurrentMa` as `0` and present `N/A` in UI.
- `hasTelemetry == false` should show clear "waiting for telemetry" state.

### 1.4 UI proposal (`/telemetry`)
- Card grid with key metrics:
  - Battery: voltage, SoC (CC), SoC (voltage)
  - Power: power limit %, estimated kW
  - Motor: temp, rpm
  - ESC: temp, current
  - System: armed/disarmed, freshness (`millis() - lastTelemetryUpdateMs`)
- Refresh strategy:
  - Poll every 1000 ms (same rhythm as `Xctod` update interval).
  - Add small status badge: `LIVE`, `STALE`, `NO DATA`.
- Actions:
  - "Back to Dashboard"
  - "Open Logs"
  - "Open Configuration"

### 1.5 Acceptance criteria
- Endpoint responds in < 50 ms typical case.
- UI updates every second without page reload.
- Matches values from logger/Xctod for same moment (within normal timing tolerance).
- Handles no-data and no-current-sensor cases gracefully.

## Task 2: Reorganize Pages

### 2.1 Problem
The current main page mixes setup/maintenance tasks (firmware), operational data management (logs), and navigation (config) in one screen.

### 2.2 New information architecture
Recommended routes:
- `/` -> Dashboard (navigation + quick status)
- `/telemetry` -> Live telemetry page
- `/firmware` -> Firmware update page
- `/logs-page` -> Log manager UI
- `/config` -> Configuration page (existing)

Keep existing API/functional routes:
- `/update` (firmware POST)
- `/list` (log list JSON)
- `/delete` (log delete)
- `/logs` (static file downloads)
- `/config/values`, `/config/save`

Rationale:
- Avoid conflict with existing static `/logs` used for file download.
- Keep backward compatibility for existing tools/scripts calling current endpoints.

### 2.3 Dashboard proposal (`/`)
Replace current mixed content with a lightweight dashboard:
- Device identity/status block
- Firmware block:
  - Current firmware version (from compile-time macro)
  - Build date/time
  - Controller type (XAG/Hobbywing/Tmotor)
  - Short health indicator (telemetry available, armed/disarmed)
- Buttons to:
  - Telemetry
  - Firmware
  - Logs
  - Configuration
- Optional quick indicators (armed, telemetry freshness, battery voltage)

### 2.4 Migration strategy
- Phase migration, no breaking API changes first.
- Keep old root behavior behind fallback only during transition if needed.
- After validation, make `/` point only to dashboard.

### 2.5 Acceptance criteria
- Each page has one clear responsibility.
- Navigation works from every page.
- Existing log download/delete and firmware upload continue working.

## Cross-cutting suggestions
- Move repeated CSS and navigation bar to shared template string helper in C++ to reduce duplication.
- Standardize naming:
  - Pages: `/telemetry`, `/firmware`, `/logs-page`, `/config`
  - APIs: `/api/...` (new telemetry endpoint should follow this)
- Add lightweight JSON error shape for APIs:
  - `{ "ok": false, "error": "message" }`
- Security hardening (incremental):
  - Basic filename validation for delete/download routes
  - Optional CSRF token for mutating requests in future iteration
- Performance budget (for lightweight UI):
  - Keep each HTML page small and avoid large inline assets.
  - Use simple CSS (no heavy frameworks).
  - Use polling interval of 1000 ms and avoid expensive DOM rebuilds.
  - Reuse common JS helpers for formatting/status updates.

## Implementation Steps (Step by Step)

### Phase 1 - Backend telemetry API
Commit message suggestion: `feat(web): add telemetry api endpoint`

- [x] Add route `GET /api/telemetry` in `ControllerWebServer::startAP()`.
- [x] Collect data from `telemetry`, `batteryMonitor`, `throttle`, `power`, and board capabilities.
- [x] Serialize JSON with stable field names and integer base units.
- [x] Add serial debug log only for error situations.
- [x] Validate no regression in existing routes (`/config`, `/update`, `/list`, `/delete`, `/logs`).

### Phase 2 - New telemetry page
Commit message suggestion: `feat(web): add live telemetry page`

- [x] Add `TELEMETRY_HTML` page constant.
- [x] Add route `GET /telemetry`.
- [x] Create frontend polling (`fetch('/api/telemetry')` every 1000 ms).
- [x] Format data for display (volts, celsius, kW).
- [x] Implement UI states (`LIVE`, `STALE`, `NO DATA`).
- [x] Keep rendering lightweight (update only changed values).

### Phase 3 - Split current mixed home
Commit message suggestion: `refactor(web): split firmware and logs pages`

- [x] Extract current firmware section into `FIRMWARE_HTML` and route `/firmware`.
- [x] Extract current logs manager section into `LOGS_HTML` and route `/logs-page`.
- [x] Keep existing firmware/log APIs unchanged.
- [x] Add consistent top navigation between pages.

### Phase 4 - New dashboard
Commit message suggestion: `feat(web): add dashboard home page`

- [x] Add `DASHBOARD_HTML` for `/`.
- [x] Include firmware info card (version, build date/time, controller type).
- [x] Add navigation links to all pages.
- [x] Show mini status summary via `/api/telemetry` (battery voltage, armed status, telemetry freshness).
- [x] Keep layout visually organized and readable on small screens.

### Phase 5 - Consistency and cleanup
Commit message suggestion: `style(web): unify ui and optimize frontend footprint`

- [ ] Unify button styles, spacing, and typography across pages.
- [ ] Remove duplicated inline JS helpers where possible.
- [ ] Ensure all pages are mobile-friendly (current viewport already exists).
- [ ] Keep only essential visual effects and avoid heavy UI libraries.

### Phase 6 - Validation checklist
- [ ] Firmware upload still works end-to-end.
- [ ] Log list/download/delete still works.
- [ ] Config read/save still works.
- [ ] Telemetry page updates reliably for all builds:
  - [ ] `lolin_c3_mini_hobbywing`
  - [ ] `lolin_c3_mini_tmotor`
  - [ ] `lolin_c3_mini_xag`
- [ ] Dashboard shows firmware info and basic status correctly.
- [ ] Captive portal fallback behavior remains functional.

## Delivery Strategy
Use a single implementation flow with one commit per phase:
- Commit 1: Phase 1
- Commit 2: Phase 2
- Commit 3: Phase 3
- Commit 4: Phase 4
- Commit 5: Phase 5
- Commit 6: Validation adjustments/fixes found in Phase 6

This sequence keeps history organized, reviewable, and sufficient without splitting into separate PRs.
