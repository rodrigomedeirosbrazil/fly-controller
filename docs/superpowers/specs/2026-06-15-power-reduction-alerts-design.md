# Power-Reduction Alerts — Design

**Date:** 2026-06-15
**Status:** Draft (pending user review)

## Goal

Alert the pilot whenever the firmware actively reduces available power. Three
mechanisms reduce power today, all in `controller/src/Power/Power.cpp`
(`calcPower()` returns the `min` of the three limits):

| Cause | Function | Notes |
|---|---|---|
| Battery voltage low | `calcBatteryLimit()` | Gated by `BoardConfig::useBatteryLimit`. Progressive floor below `getBatteryMinVoltage()`. |
| Motor temperature high | `calcMotorTempLimit()` | Linear reduction `reductionStart`→`maxTemp`. |
| ESC temperature high | `calcEscTempLimit()` | Linear reduction `reductionStart`→`maxTemp`. |

These are the only three power-reduction sources. When any of them drops power
below 100%, the pilot should get:

1. An audible alert on **all sound devices** — controller buzzer, remote-throttle
   buzzer, and the telemetry web page (Web Audio).
2. On the telemetry page: a **persistent red highlight** on the power card and
   on the offending metric card(s), plus a **dismissible alert card** naming the
   cause(s).

## Behavior

- **When active:** only while `throttle.isArmed()` *and* at least one limiter is
  actively reducing power. The conservative 50% startup floor (returned by
  `calcBatteryLimit()` when telemetry is not yet available) does **not** count as
  a cause, so it never false-alarms.
- **Beep cadence:** one beep immediately on entering the limited state, then a
  repeat every **10 s** for as long as any cause persists. Stops when power
  returns to 100% or the system disarms.
- **Single alert tone:** the same distinct beep pattern for any cause; the web
  page identifies *which* cause. The beep is clearly different from the existing
  armed alert (2000 Hz) and arming-blocked beeps.

## Architecture

### 1. Cause tracking in `Power`

`calcPower()` already computes `batteryLimit`, `motorTempLimit`, `escTempLimit`.
Add a bitmask recording which limiters are *actively* reducing power, refreshed
on each recompute (the existing 500 ms cache in `getPower()`).

```cpp
enum PowerLimitCause : uint8_t {
  POWER_LIMIT_NONE       = 0,
  POWER_LIMIT_BATTERY    = 1 << 0,
  POWER_LIMIT_MOTOR_TEMP = 1 << 1,
  POWER_LIMIT_ESC_TEMP   = 1 << 2,
};
```

Rules inside `calcPower()`:
- Battery cause set only when `useBatteryLimit && telemetry.hasData() && batteryLimit < 100`
  (the `hasData()` guard excludes the conservative startup floor).
- Motor / ESC cause set when their respective limit `< 100` (those functions
  already return 100 when telemetry is missing or the reading is out of valid
  range).
- When `getPowerControlEnabled() == false`, the mask is `NONE`.

New getter: `uint8_t Power::getActiveLimitCauses() const;`

### 2. `PowerAlert` component (new) + host-tested logic

Follows the project convention of putting pure decision logic in a host-testable
header (like `RemoteLink/RemoteLinkLogic.h` and `Power`'s existing test).

`controller/src/PowerAlert/PowerAlertLogic.h` — pure, no Arduino deps:

```cpp
class PowerAlertLogic {
public:
  // Returns true when a beep should be emitted this tick.
  // limited = armed && activeCauses != 0
  //   - fires immediately on the transition into the limited state
  //   - then fires every intervalMs while it persists
  //   - resets when no longer limited
  bool update(bool armed, uint8_t activeCauses, uint32_t nowMs, uint32_t intervalMs);
private:
  bool wasLimited = false;
  uint32_t lastBeepMs = 0;
};
```

`controller/src/PowerAlert/PowerAlert.{h,cpp}` — the component:
- `handle()` is called from `loop()`. It reads `throttle.isArmed()`,
  `power.getActiveLimitCauses()`, and `millis()`, then calls
  `logic.update(..., POWER_ALERT_BEEP_INTERVAL_MS)`.
- When `update()` returns true it (a) increments an internal `seq` counter,
  (b) calls `buzzer.beepPowerAlert()`, and (c) calls
  `remoteLink.requestBeep(RemoteBeep::PowerAlert)`.
- Exposes `uint32_t getAlertSeq() const;` and `uint8_t getActiveCauses() const;`
  (the latter just forwards `power.getActiveLimitCauses()`, cached at last
  `handle()`) for the web layer.

`POWER_ALERT_BEEP_INTERVAL_MS = 10000` as a `#define` in `config.h` (not a
persisted setting — YAGNI; easy to tune later).

Wire-up (per `config.h`/`config.cpp` convention): `extern PowerAlert powerAlert;`
in `config.h`, instantiate in `config.cpp`, call `powerAlert.handle()` in
`loop()`.

### 3. Distinct alert beep (all devices)

- **Controller** (`controller/src/Buzzer/Buzzer.{h,cpp}`): add
  `beepPowerAlert()` — a distinct pattern (e.g. three rapid ~2500 Hz beeps),
  clearly different from `beepArmedAlert()`. This automatically lands in the
  `BeepEvent` ring, so the **telemetry page** replays it via Web Audio with no
  extra plumbing.
- **Protocol** (`shared/RemoteLinkProtocol.h`): add `PowerAlert = 7` to the
  `RemoteBeep` enum.
- **Remote** (`throttle/src/main.cpp` + `throttle/.../Buzzer`): map
  `RemoteBeep::PowerAlert` to a matching `beepPowerAlert()` on the remote buzzer.

### 4. Web telemetry

`/api/telemetry` (`controller/src/WebServer/ControllerWebServer.cpp`) gains a
`powerAlert` object:

```json
"powerAlert": {
  "seq": 12,
  "causes": ["motorTemp", "escTemp"]
}
```

- `seq` = `powerAlert.getAlertSeq()`, bumped in lockstep with each beep (entry +
  every 10 s). Synchronizes the web alert card with the audible beep.
- `causes` = array of active-cause keys (`"battery"`, `"motorTemp"`, `"escTemp"`),
  derived from the bitmask. Empty when nothing is limiting.

`TelemetryPage.h` rendering (matches the approved mockup):

**Persistent highlights — driven directly by `causes` (shown the whole time a
cause is active):**
- The power card ("Energia" → the "Limite" % badge) turns red whenever `causes`
  is non-empty.
- Each active cause's card turns red with a ⚠ marker:
  `battery`→"Tensão" card, `motorTemp`→"Motor" card, `escTemp`→"ESC" card.
- Cleared automatically when `causes` becomes empty.

**Dismissible alert card — driven by `seq` (event, in-flow, never overlaps
data):**
- An in-flow card between the status bar and the grid, styled like the existing
  `.wake-panel` (white card, shadow, pushes content down — does **not** overlay).
  Red accent, titled "Potência reduzida", lists the active cause(s) in
  Brazilian Portuguese (e.g. "Temperatura do motor alta · Temperatura do ESC
  alta"), with a ✕ close button.
- Page tracks `lastAlertSeq` and `dismissedSeq`:
  - On poll, if `causes` is empty → hide the card, reset `dismissedSeq`.
  - Else if `seq` changed and `seq != dismissedSeq` → show the card; set
    `lastAlertSeq = seq`.
  - ✕ sets `dismissedSeq = lastAlertSeq` and hides the card. The next fire (10 s
    later) bumps `seq`, so the card reopens.

Cause→Portuguese label map (user-facing strings stay in PT per project
convention):
- `battery` → "Tensão da bateria baixa"
- `motorTemp` → "Temperatura do motor alta"
- `escTemp` → "Temperatura do ESC alta"

### Error / edge handling

- Multiple simultaneous causes: all listed in the card and all their cards
  highlighted; a single beep (single tone covers all).
- Telemetry buffer: `powerAlert` adds a small object to the existing
  `StaticJsonDocument<2048>`; verify no overflow (existing `doc.overflowed()`
  check already guards).
- `requestBeep()` is harmless when no remote is paired.

## Testing

- `controller/test/PowerAlertLogicTest.cpp` (compiled like `PowerTest.cpp` with
  `c++ -std=c++17`), covering:
  - not armed → never beeps, even with active causes;
  - armed + cause → immediate beep on entry;
  - no re-beep before `intervalMs`; beep again at/after `intervalMs`;
  - causes clear → resets; re-entry beeps immediately again;
  - multi-cause behaves as a single limited state.
- Build all three controller targets (`_hobbywing`, `_tmotor`, `_xag`) and the
  remote firmware (`remote_throttle`).

## Files touched

- `controller/src/Power/Power.{h,cpp}` — cause bitmask + getter.
- `controller/src/PowerAlert/{PowerAlertLogic.h,PowerAlert.h,PowerAlert.cpp}` — new.
- `controller/src/config.{h,cpp}` — extern/instantiate `powerAlert`, interval `#define`.
- `controller/src/main.cpp` — call `powerAlert.handle()` in `loop()`.
- `controller/src/Buzzer/Buzzer.{h,cpp}` — `beepPowerAlert()`.
- `shared/RemoteLinkProtocol.h` — `RemoteBeep::PowerAlert = 7`.
- `throttle/src/main.cpp` + `throttle/.../Buzzer` — remote `beepPowerAlert()` mapping.
- `controller/src/WebServer/ControllerWebServer.cpp` — `powerAlert` JSON.
- `controller/src/WebServer/Pages/TelemetryPage.h` — highlights + alert card.
- `controller/src/WebServer/Pages/CommonStyles.h` — alert-card / highlight CSS.
- `controller/test/PowerAlertLogicTest.cpp` — new host test.

## Out of scope (YAGNI)

- Per-cause distinct beep tones (single tone chosen).
- Configurable beep interval as a persisted setting (constant for now).
- Alerting while disarmed.
