# Fly Control BLE Protocol

The **Fly Control** GATT service is the controller's own binary protocol, used
by the [fly-app](https://github.com/rodrigomedeirosbrazil/fly-app) Flutter
client. It carries telemetry, configuration and one-off actions.

It exists alongside the Nordic UART service documented in
[XCTOD-PROTOCOL.md](XCTOD-PROTOCOL.md), which is **frozen**: that CSV sentence
has two positional consumers (XCTrack's hand-written `.xcfg` and fly-app's
`xctod_parser.dart`) with no version handshake, so no field is ever added to
it. Everything new lives here instead.

The definition of record is `src/BleControl/ControlProtocol.h`, host-tested by
`test/ControlProtocolTest.cpp`. **This document describes that header; the
header wins if they ever disagree.**

## Implementing the client side

The header is duplicated by hand as Dart in the fly-app repo. There is no
shared package and no code generation, which means:

- Both copies carry `CONTROL_PROTOCOL_VERSION`. Bump it in both repos, in the
  same change, whenever an existing field moves or changes meaning.
- `test/ControlProtocolTest.cpp` here pins every struct offset and every enum
  value. The Dart side should pin the same numbers. Those tests are the only
  thing standing between a field reorder and a phone silently decoding garbage.

Everything is **little-endian** and every struct is `#pragma pack(1)` — no
padding anywhere.

## Discovery

| | |
|---|---|
| Service UUID | `D4CF0001-9B9D-4BFD-8F7F-40C6989D3EA9` |
| Advertised | **No** |

The 31-byte advertising payload cannot hold a second 128-bit UUID, so only the
NUS service UUID is advertised. Connect first, then discover.

**Service presence is the capability handshake.** Firmware older than this
feature simply does not have the service; a client that does not find it should
fall back to parsing the `$XCTOD` sentence. Do not delete that fallback.

| Characteristic | UUID | Properties |
|---|---|---|
| `INFO` | `D4CF0002-9B9D-4BFD-8F7F-40C6989D3EA9` | READ |
| `TELEMETRY` | `D4CF0003-9B9D-4BFD-8F7F-40C6989D3EA9` | NOTIFY |
| `CMD` | `D4CF0004-9B9D-4BFD-8F7F-40C6989D3EA9` | WRITE |
| `RSP` | `D4CF0005-9B9D-4BFD-8F7F-40C6989D3EA9` | NOTIFY |

`D4CF0006-…` is reserved for the phase-3 bulk channel (log download, OTA) and
is not implemented.

Request an MTU of 247. Every frame in this protocol fits in one packet at that
size, so there is no fragmentation layer. Measured working on a Galaxy A12
(API 31).

## `INFO` — 28 bytes, read once

Static for the whole session; the firmware writes it at boot.

| Offset | Type | Field |
|---|---|---|
| 0 | `uint8` | `protocolVersion` — `CONTROL_PROTOCOL_VERSION` |
| 1 | `uint8` | `controllerType` — 1 = XAG, 3 = Tmotor |
| 2 | `uint16` | `capabilities` |
| 4 | `char[24]` | `appVersion` — firmware version, NUL-padded |

Capability bits:

| Bit | Meaning |
|---|---|
| `0x0001` | CAN telemetry (RPM and current are real) |
| `0x0002` | Battery voltage sensor present |
| `0x0004` | Motor temp source is selectable |
| `0x0008` | Remote throttle link supported |

`protocolVersion` is a **diagnostic, not a compatibility gate.** It is bumped
only when an existing field moves or changes meaning — precisely the case the
append rule does *not* rescue, since appending saves nothing if offsets
shifted underneath. Gate on the telemetry struct's own `ver` and on the
received length instead, and log `protocolVersion` for support.

## `TELEMETRY` — 58 bytes at 1 Hz

Two rules govern this struct, and together they are what replaces the CSV.

**1. Fixed layout. An unavailable reading is zero with its bit clear, never an
absent field.** Every offset below is constant forever.

**2. Fields are only ever appended at the end, and the reader parses
`min(received, known)`.** New firmware with an old client: the client reads the
prefix it understands and ignores the tail. Old firmware with a new client: the
client sees a short packet and treats the fields beyond it as absent. Neither
breaks. **Decode by offset against the received length — never assume 58.**
`stateFreqHz` was appended after the first release and is the worked example:
firmware without it sends 56 bytes and nothing else changes.

| Offset | Type | Field | Notes |
|---|---|---|---|
| 0 | `uint8` | `ver` | struct version |
| 1 | `uint8` | `flags` | see below |
| 2 | `uint16` | `validity` | see below |
| 4 | `uint8` | `disarmReason` | see below |
| 5 | `uint8` | `signalStates` | see below |
| 6 | `uint8` | `motorTempSrc` | 0 = none, 1 = CAN, 2 = NTC |
| 7 | `uint8` | `socCc` | % — coulomb counting |
| 8 | `uint8` | `socVolt` | % — from voltage |
| 9 | `uint8` | `throttlePct` | |
| 10 | `uint8` | `powerPct` | available power after derating |
| 11 | `uint8` | `powerScale` | disarm-ramp scale |
| 12 | `uint8` | `armCharge` | arming-gesture progress |
| 13 | `uint8` | `limitCauses` | see below |
| 14 | `uint16` | `batteryMv` | |
| 16 | `uint16` | `throttleRaw` | raw ADC |
| 18 | `uint16` | `powerKwX10` | kW x 10 |
| 20 | `int32` | `escCurrentMa` | signed: regen is legitimate |
| 24 | `uint32` | `rpm` | |
| 28 | `int32` | `motorTempMc` | millicelsius |
| 32 | `int32` | `escTempMc` | millicelsius |
| 36 | `uint32` | `sessionSec` | flight clock, RAM-only |
| 40 | `uint32` | `hourMeterSec` | lifetime motor-run, persisted |
| 44 | `uint16` | `bmsCellMinMv` | |
| 46 | `uint16` | `bmsCellMaxMv` | |
| 48 | `uint16` | `bmsCellDeltaMv` | |
| 50 | `int16` | `bmsTempMaxC` | |
| 52 | `uint32` | `uptimeSec` | |
| 56 | `uint16` | `stateFreqHz` | state-layer tone in Hz, 0 when none |

Temperatures are millicelsius because that is the unit the firmware uses
everywhere; decigrees would have saved 8 bytes and bought a class of conversion
bug.

### `flags`

| Bit | Meaning |
|---|---|
| `0x01` | Armed |
| `0x02` | Throttle engaged (past the engage hysteresis) |
| `0x04` | Has telemetry |
| `0x08` | Power control enabled |
| `0x10` | BMS connected |
| `0x20` | BMS configured |

### `validity` — availability, not health

This mask answers "does this build and configuration produce this reading at
all", which is a different question from whether a sensor is currently healthy.

| Bit | Meaning |
|---|---|
| `0x0001` | `escCurrentMa` is real |
| `0x0002` | `rpm` is real |
| `0x0004` | `powerKwX10` is real |
| `0x0008` | BMS data present |
| `0x0010` | BMS per-cell data present |

Motor temp, ESC temp and battery voltage deliberately have **no** bit here —
their health is in `signalStates`. A bit would be a second answer to the same
question, populated from a second call site and free to drift. SoC has no bit
either: the firmware has no validity concept for it (`socVolt` follows the
battery-voltage signal state; coulomb counting has none).

Eleven bits are spare. Adding one later is **not** a breaking change.

### `signalStates` — sensor health, 2 bits each

| Bits | Signal |
|---|---|
| 5–4 | Motor temperature |
| 3–2 | ESC temperature |
| 1–0 | Battery voltage |

Each value: `0` Absent (not in this build), `1` Stale (no recent reading),
`2` Invalid (physically impossible reading), `3` Valid.

Only `3` means the number can be trusted. Zero is a legitimate reading, so a
client must check the state rather than the value.

### `limitCauses` — which limiter is acting

| Bit | Cause |
|---|---|
| `0x01` | Battery voltage |
| `0x02` | Motor temperature |
| `0x04` | ESC temperature |

Non-zero means `powerPct` is below 100 for that reason.

### `disarmReason`

| Value | Code | Meaning |
|---|---|---|
| 0 | — | never disarmed (internal sentinel) |
| 1 | `MANUAL` | pilot disarmed |
| 2 | `THR ERR` | wired throttle reading invalid |
| 3 | `LINK ERR` | wireless throttle link lost |
| 4 | `MOT ERR` | motor temp sensor lost mid-flight |
| 5 | `ESC ERR` | ESC temp sensor lost mid-flight |
| 6 | `BATT ERR` | battery voltage lost mid-flight |
| 7 | `MOT SRC` | motor temp source changed mid-flight |

Values 2–7 are faults and should stay visible until the pilot re-arms.

## `CMD` / `RSP` — the request protocol

```
CMD  →  [op u8][seq u8][len u8][payload len]
RSP  ←  [op u8][seq u8][status u8][len u8][payload len]
```

`seq` is echoed so a response can be matched to its request. **Start at 1:
`seq = 0` is reserved** for unsolicited events on `RSP`.

Malformed frames get no reply at all — the firmware cannot know which `seq` to
answer. Time out and retry.

### Status codes

| Value | Name | Meaning |
|---|---|---|
| 0 | `Ok` | |
| 1 | `ErrAuth` | PIN not presented on this connection |
| 2 | `ErrBadOp` | unknown opcode |
| 3 | `ErrBadArg` | payload malformed or out of range |
| 4 | `ErrState` | refused in the current state (armed) |
| 5 | `ErrBusy` | a long-running operation holds the resource |

`ErrBadOp` is what lets a newer client probe older firmware and degrade rather
than hang. Treat it as "this firmware cannot do that", not as an error to show.

### Authentication

Reads are open; writes need the PIN. This mirrors the web portal, where every
`GET` is free and every `POST` checks the PIN.

- `TELEMETRY`, `INFO` and `CFG_GET` work unauthenticated — show the flight
  panel without ever prompting.
- Everything else answers `ErrAuth` until `AUTH` succeeds.
- **Authentication is per connection and is cleared on disconnect.** Re-send
  `AUTH` after any reconnect.
- **A failed `AUTH` clears an existing session.** Fail closed is deliberate;
  do not retry speculatively with a guessed PIN.

`AUTH` payload is the PIN as raw characters, not NUL-terminated.

### The armed gate

**While the aircraft is armed, requests are refused by default** with
`ErrState`. Only these pass:

| Opcode | Why |
|---|---|
| `AUTH` (`0x01`) | authenticating changes nothing |
| `CFG_GET` (`0x10`) | read-only |
| `BMS_SCAN_STATUS` (`0x22`) | read-only |
| `SESSION_RESET` (`0x20`) | a RAM-only counter that cannot reach the motor |

**`ErrState` is reported before `ErrAuth`**, so a client should never prompt for
a PIN in response to it — the request would be refused either way.

Note `BMS_DETECT` is *not* in that list despite reading like a query: it drops
the BMS link and blocks on a BLE connect, which can outlast the 10 s task
watchdog and reboot the controller. In flight that cuts the motor.

### Opcodes

| Op | Name | Payload → Response |
|---|---|---|
| `0x01` | `AUTH` | PIN chars → — |
| `0x10` | `CFG_GET` | `[group u8]` → group struct |
| `0x11` | `CFG_SET` | `[group u8][struct prefix]` → — |
| `0x20` | `SESSION_RESET` | — → — |
| `0x21` | `BMS_SCAN_START` | — → — (`ErrBusy` if already scanning) |
| `0x22` | `BMS_SCAN_STATUS` | — → see below |
| `0x23` | `BMS_DETECT` | `[mac 6]` → `[type u8]` |
| `0x24` | `REMOTE_PAIR` | — → — |
| `0x25` | `REMOTE_FORGET` | — → — |
| `0x26` | `BUZZER_PREVIEW` | `[volume u8]` (0–100) → — |
| `0x27` | `SET_TIME` | `[epochMs i64]` → — |
| `0x28` | `PIN_CHANGE` | `[curLen u8][cur…][newLen u8][new…]` → — |
| `0x29` | `TMOTOR_DIR_FORWARD` | — → — (Tmotor only) |
| `0x2A` | `TMOTOR_DIR_REVERSE` | — → — (Tmotor only) |

On XAG the two direction opcodes return `ErrBadOp`.

`SET_TIME` rejects anything at or below `1577836800000` (2020-01-01).
`PIN_CHANGE` requires the new PIN to be 4–8 characters and answers `ErrAuth` if
the current one is wrong.

`BMS_SCAN_STATUS` returns `[status u8][count u8]` then, per result,
`[mac 6][rssi i8][type u8]`. The list is truncated to fit one frame while
`count` still reports the true total — so `count` may exceed the entries
present. The web portal's richer scan payload (device name, advertised
services) is not carried here.

### Configuration groups

`CFG_GET` and `CFG_SET` work on whole groups, not individual keys, because
values like the thermal reduction start and maximum are only meaningful
validated as a pair.

| Group | Id | Size | Fields |
|---|---|---|---|
| Power | 0 | 9 | `uint16` capacityMah, `uint16` minVoltageMv, `uint16` maxVoltageMv, `uint8` powerControlEnabled, `uint16` dividerRatioX100 |
| Thermal | 1 | 17 | `int32` motorReductionStartMc, `int32` motorMaxMc, `int32` escReductionStartMc, `int32` escMaxMc, `uint8` motorTempSource |
| Bms | 2 | 7 | `uint8` bmsType, `uint8[6]` bmsMac |
| System | 3 | 8 | `uint8` buzzerVolume, `uint8` throttleSource, `uint8[6]` remoteMac |

Two encodings differ from how the firmware stores them, because the stored
types cannot be sent raw:

- **`dividerRatioX100`** is the voltage divider ratio times 100.
- **MAC addresses are 6 raw bytes**, not the 17-character text form. All zero
  means unset.

`bmsType`: 0 none, 1 JBD, 2 Daly, 3 JK. A non-zero type requires a non-zero
MAC or the write is rejected. `throttleSource`: 0 wired, 1 wireless.
`motorTempSource`: 0 CAN, 1 NTC — only meaningful when the capability bit is
set.

**`CFG_SET` applies only the prefix it receives.** The firmware seeds the
struct from current values and overlays what arrived, so an older client
sending a short struct updates the fields it knows and leaves the rest alone.
A client may therefore send a truncated struct deliberately. Ranges are
validated against the same code the web portal uses; a failure is `ErrBadArg`
and nothing is written.

### Events

An `RSP` notification with `seq = 0` is unsolicited.

| Op | Event | Payload |
|---|---|---|
| `0x80` | `EVT_BEEP` | 13 bytes, see below |

| Offset | Type | Field |
|---|---|---|
| 0 | `uint32` | `seq` — monotonic |
| 4 | `uint16` | `frequency` (Hz) |
| 6 | `uint16` | `onMs` |
| 8 | `uint16` | `offMs` |
| 10 | `uint8` | `reps` — 0 = continuous |
| 11 | `uint8` | `layer` — 0 = event, 1 = persistent state |
| 12 | `uint8` | `active` — 1 = started, 0 = stopped |

Beeps are **pushed at the moment they happen**, not polled — unlike the web
telemetry page, which polls a ring buffer and de-duplicates in JavaScript.

There is **no backfill**: a client that connects mid-flight never sees beeps
that happened before it connected. This is a live feed, not a log.

The `layer` field matters for playback. Layer 0 is a momentary event; layer 1
is a persistent state whose `active` flag toggles a looping tone. An event
should pause a running state tone and resume it afterwards — see
`initBuzzerMirror` in the web telemetry page for a working implementation of
the same policy.

**A layer-1 event carries the tone's starting frequency, not its current
one.** The arm-charge and disarm-ramp gestures sweep between 1800 and 2500 Hz,
stepping on every on-to-off edge, and those steps push no event — the ring is
8 slots and the patterns pulse at 60/40 ms, so an event per step would
overwrite itself between notifications. Drive the looping tone's pitch from
`stateFreqHz` in the 1 Hz telemetry instead; the `active` flag still says
whether it should be playing at all.

## Coexistence

The controller's single radio runs a WiFi AP, ESP-NOW to the remote throttle, a
BLE client to the BMS, and this BLE server. `CONFIG_BT_ACL_CONNECTIONS` is 4,
so XCTrack and the app can both be connected while the BMS link is up.

Advertising restarts on connect, which is what makes a second central possible
at all — Bluedroid stops advertising once the first one connects.

Advertising is suppressed entirely while a BMS scan runs. A client that
disconnects during a scan will not find the controller until the scan finishes.

## Firmware update

Bulk data does **not** go through `CMD`: the queue is four deep, 32 bytes a
slot, drop-newest — a 1.8 MB image would be ~57,000 round trips. It goes to
`D4CF0006-9B9D-4BFD-8F7F-40C6989D3EA9`, **write without response**, as
`[offset u32 LE][data…]`.

Write-without-response is what makes the transfer take a minute rather than
ten. It guarantees nothing, which is why the offset is absolute and in every
packet and the image carries a CRC32.

| Op | Name | Payload → Response | Auth | While armed |
|---|---|---|---|---|
| `0x50` | `DFU_BEGIN` | `[size u32][crc32 u32]` → `[chunkSize u16]` | yes | refused |
| `0x51` | `DFU_COMMIT` | — → — | yes | refused |
| `0x52` | `DFU_ABORT` | — → — | yes | refused |
| `0x53` | `DFU_STATUS` | — → `[state u8][received u32][chunkSize u16]` | **no** | **allowed** |

`state`: `0` idle · `1` receiving · `2` verifying · `3` ready · `4` error.

`chunkSize` is the usable bytes per data packet — the **negotiated** ATT MTU
minus 3. Subtract 4 more for the offset header to get image bytes per packet.
It is reported by both `DFU_BEGIN` and `DFU_STATUS`, so a client that
negotiated its MTU after `DFU_BEGIN` can pick up the larger value.

### Receiving

The controller tracks the **highest contiguous offset accepted**, and that is
what `received` reports. A packet whose offset is not exactly `received` is
**dropped, not buffered** — below it is a resend, above it is a gap, and in
both cases the client is expected to restart from `received` rather than the
controller repairing the sequence. A packet that would overshoot `size` is
dropped too.

Dropping also happens when the controller's staging buffer is momentarily
full. There is no signal for it: poll `DFU_STATUS`, see `received` has not
moved, and resume from there.

### Committing

`DFU_COMMIT` is refused with `ErrState` unless `received == size` **and** the
CRC32 accumulated over the received image matches the one from `DFU_BEGIN`.

On success the controller answers `Ok` and then restarts on a later tick — the
answer comes first, so a successful update is never reported as a failure.
Expect the link to drop immediately after.

Note the CRC is accumulated over the bytes as they arrive, not computed by
reading the flash back. Reading back is impossible here: the ESP32 updater
withholds the first 16 bytes of the image until the transfer is finalised, so
that a partial image is never bootable, and a read-back before that point
would never match.

### What neither side can check

Nothing in an ESP32 image says which controller it is for. XAG and Tmotor run
different builds and both pass the magic byte, the size and the CRC. Warn the
pilot; the recovery is a USB cable.

## Not implemented

Log download over BLE. Opcode range `0x40–0x4F` is reserved for it.
