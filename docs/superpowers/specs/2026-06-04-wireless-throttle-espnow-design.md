# Wireless Throttle over ESP-NOW — Design

**Date:** 2026-06-04
**Status:** Approved (design)
**Branch:** `feature/wireless-throttle-espnow`

## Overview

Today the controller reads the throttle from a wired Hall sensor (GPIO0 / ADS1115 ch0) and the
action button from GPIO5. This feature introduces a **second ESP32** — the *remote throttle* — that
reads a Hall sensor and a push button locally and transmits them to the controller over **ESP-NOW**.

The controller keeps **all** existing throttle logic (calibration, arming state machine, percentage
computation, ramping, power limiting). In wireless mode, the throttle's `ReadFn` simply returns the
last raw Hall value received over ESP-NOW instead of reading the local ADC. Button events received
over the link feed the existing `handleButtonEvent()` path. The controller sends state back to the
remote (armed/disarmed for two LEDs, plus beep commands).

The **wired throttle remains fully supported**. The input source (wired vs wireless) is selected in
the web portal and stored in `Settings`.

### Key principle: the remote is "dumb"

The remote throttle is a minimal sensor node. It owns **no** calibration or arming logic. It only:
- reads the Hall sensor and sends the **raw** value,
- runs AceButton and sends **button events** (short click / long press),
- drives two status LEDs (red = armed, green = disarmed) from received state,
- plays beeps from received beep commands.

The controller remains the single source of truth for calibration, arming, and percentage. This maps
directly onto the existing `ReadFn` pattern, maximizing code reuse and avoiding duplicated logic.

## Repository Structure (monorepo)

The repository is reorganized into two PlatformIO projects sharing a protocol header:

```
controller/                     # current firmware (moved from src/ + platformio.ini)
throttle/                       # new remote-throttle firmware
shared/
  RemoteLinkProtocol.h          # ESP-NOW packet structs + enums, included by both
```

Feature development happens on `feature/wireless-throttle-espnow`. Moving the current project into
`controller/` requires updating build/CI paths in `.github/workflows/build-and-release.yml`, the
root `CLAUDE.md`, and `src/CLAUDE.md`.

> A long-lived parallel branch for the remote firmware was explicitly rejected: the packet struct
> must stay byte-identical on both sides, and keeping that synchronized across branches is a
> guaranteed source of bugs. A single shared header in one repo is the correct coupling.

## ESP-NOW Protocol (`shared/RemoteLinkProtocol.h`)

- **Fixed radio channel = 1**, matching the softAP default. ESP-NOW and the WiFi AP must operate on
  the same channel on the ESP32-C3's single shared radio.
- **TX power = 8.5 dBm** on both devices (see *Radio Coexistence* below).

### Remote → Controller (~50 Hz)

```c
struct ThrottleToControllerPacket {
    uint16_t hallRaw;            // raw Hall ADC reading
    uint8_t  buttonEventCounter; // monotonic; increments once per discrete button event
    uint8_t  buttonEventType;    // enum: None / ShortClick / LongPress
};
```

The `buttonEventCounter` deduplicates button events without ACKs: the controller acts only when the
counter changes. This makes button delivery robust against the 50 Hz repetition and packet loss.

### Controller → Remote (heartbeat ~5 Hz + on-change)

```c
struct ControllerToThrottlePacket {
    uint8_t armed;              // drives LEDs (red = armed, green = disarmed)
    uint8_t calibrating;        // controller is in calibration sweep
    uint8_t beepCommandCounter; // monotonic; increments once per beep request
    uint8_t beepCommand;        // enum mapping to named Buzzer patterns
};
```

`beepCommand` is deduplicated by `beepCommandCounter`, same scheme as button events.

## Radio Coexistence (ESP-NOW + WiFi AP + BLE)

The controller **already** runs WiFi AP (web portal) and BLE (Xctod server + BluetoothBms) together
on the C3's single 2.4 GHz radio via time-division. ESP-NOW joins on the WiFi side. Constraints:

1. **Same channel:** ESP-NOW peers and the softAP must share a channel — fixed to channel 1.
2. **WiFi stays on:** the dead `ControllerWebServer::stop()` / `WIFI_OFF` path is removed (see
   *Cleanups*), so WiFi is up for the whole session and ESP-NOW is never torn down underneath.
3. **Airtime:** throttle packets are tiny and low-rate; Xctod sends ~1 Hz. The only heavy consumer
   is the web-triggered active BLE scan in BluetoothBms — acceptable since it is operator-initiated.

### Why TX power is reduced to 8.5 dBm

Commit `f06aa0d` (2026-03-20) set `WiFi.setTxPower(WIFI_POWER_8_5dBm)` to **avoid unstable WiFi on
the ESP32-C3 Supermini, which has problems transmitting at maximum power**. ESP-NOW inherits this
same radio and TX power. Because the remote and controller sit very close together, 8.5 dBm is more
than enough range. This rationale must be documented inline at the call site and in `CLAUDE.md`
(currently undocumented).

## Failsafe — link loss (controller side)

Active only in wireless mode. The controller tracks `lastRxMillis`:

- **No packet for > 500 ms** → ramp throttle to zero (respecting the existing ramp-down rate),
  **remain armed**. A short glitch does not force a re-arm.
- **No packet for > 3 s** → **disarm**. Re-arming then requires a button event.

## Failsafe — link loss (remote side)

If the remote stops receiving controller state for a timeout, it indicates **link lost** on the LEDs
(both blinking together) and plays a local alert beep, giving the pilot immediate feedback.

## Remote LED State Machine

| State | Red | Green |
|---|---|---|
| **Unpaired** (powered on, no saved peer / no link) | blinking | off |
| **Pairing mode** | alternating | alternating (red ↔ green) |
| **Paired + disarmed** | off | solid |
| **Paired + armed** | solid | off |
| **Link lost** (was paired, stopped receiving) | blinking together | blinking together |

The three blink patterns are distinguishable: unpaired = red only; pairing = alternating;
link-lost = both in sync.

## Pairing

ESP-NOW addresses by MAC. Pairing flow:

1. Operator puts the controller into **pairing mode** via the web portal.
2. Operator holds the remote's button for a few seconds → the remote broadcasts its MAC.
3. The controller captures the MAC and saves it to `Settings`.
4. Thereafter both communicate only with the saved peer.

## Controller-side Changes

- **New component `RemoteLink/`** — ESP-NOW setup, RX/TX, last-state buffer, RX timestamps,
  pairing-mode handling. Follows the standard component pattern (`extern` in `config.h`, instantiated
  in `config.cpp`, `setup()`/`handle()` from `main.cpp`).
- **`Settings`** — add `throttleSource` (wired/wireless) and the paired remote MAC.
- **Throttle wiring** — in wireless mode, the `throttle` `ReadFn` returns `remoteLink.lastHallRaw()`.
  Received button events feed the existing `handleButtonEvent()` (short = arm/disarm, long =
  calibration). Calibration works unchanged because raw values flow in over the link, and calibration
  step beeps are forwarded to the remote.
- **Buzzer** — both buzzers stay active: the controller plays its local beeps and forwards beep
  commands to the remote.
- **Web portal** — throttle-source selector and a pairing-mode control.

## Remote-side Firmware (`throttle/`)

Minimal application:
- Hall sensor via ADC using the `ReadFn` pattern.
- AceButton on the button GPIO (short click / long press), reusing the existing event semantics.
- ESP-NOW TX (throttle + button) and RX (state + beeps).
- Two status LEDs driven by the LED state machine above.
- Buzzer (reuses the existing `Buzzer` class) driven by received beep commands + local link-lost
  alert.
- No WiFi, no web portal.
- TX power 8.5 dBm (same C3 Supermini workaround).

Shared code (`Buzzer`, the Hall `ReadFn` reading style, button event semantics) is reused where it
cleanly applies; the protocol header is the single shared contract.

## Cleanups (in scope)

- Remove `ControllerWebServer::stop()` and the `WIFI_OFF` path — it has **no callers** in the current
  codebase (dead code). WiFi powers on at boot and stays on for the whole session.
- Document the `setTxPower(WIFI_POWER_8_5dBm)` rationale (C3 Supermini instability, commit `f06aa0d`)
  inline and in `CLAUDE.md`.

## Out of Scope (YAGNI)

- ESP-NOW payload encryption.
- Multiple simultaneous remotes.
- Reverse telemetry to a remote display.
- OTA updates for the remote firmware.

## Testing

- **Protocol header**: struct sizes/layout assertions; counter-based dedup logic unit-testable in
  isolation.
- **Failsafe timing**: simulate `lastRxMillis` gaps and assert ramp-to-zero at >500 ms and disarm at
  >3 s.
- **LED state machine**: table-driven mapping from (paired, pairing, armed, linkLost) → LED pattern.
- **End-to-end**: bench test with two devices — pairing, arming via remote button, throttle sweep,
  calibration, link-loss behavior (power off the remote), and WiFi/BLE coexistence with the portal
  open.
