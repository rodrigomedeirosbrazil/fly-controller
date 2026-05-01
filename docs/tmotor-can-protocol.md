# T-Motor ESC — CAN Bus Protocol Analysis

Two complementary sources of truth:

1. **Official documentation** — `TMOTOR ESCs CAN Communication Manual V2.2.0.pdf`
   (covers both DRONECAN-S §3 and CUBECAN §4) and `t-motor-uavcan-2.2.pdf` /
   `t-motor-uavcan-2.3.pdf`. Defines the protocol, the official
   `Enable Reporting` Generic Instruction (msg_id 4670), and the parameter
   read/write commands (rotation direction, throttle priority, etc.).
2. **Live captures** — sequence of captures on a 1 Mbps bus between a T-Motor
   ESC and the CloudLink configurator app, taken with an ESP32-C3 sniffer
   (`feature/can-monitor`). They show that CloudLink:
   - emits both DRONECAN-S **and** CUBECAN traffic (the ESC broadcasts
     CUBECAN status frames in parallel with DroneCAN);
   - does **not** use the official Enable Reporting or the documented
     CUBECAN `OPE_ID` (`0x10000106`) for either Status upload or direction
     change. Instead it uses an undocumented multi-frame SET on
     DroneCAN DataTypeId 1000, internal `msg_id = 0x1247`, with section
     numbers `0x02`/`0x04`/`0x10`.

Three discovered uses of the proprietary `msg_id = 0x1247` SET:

| Section | Frames | What it sets                                      |
|---------|-------:|---------------------------------------------------|
| `0x02`  | 3      | "Begin write" handshake (always sent first)        |
| `0x04`  | 10     | General ESC config — PWM range, voltage limits, **rotation direction (body[2..3])** |
| `0x10`  | 8      | Status upload Open/Close (byte 28 of inner data)   |

Sections `0x02`+`0x04` together implement **real-time motor direction switch
without reboot** — the topic that motivated `feature/tmotor-direction-change`.
Sections `0x02`+`0x04`+`0x10` together implement the Status upload toggle
(persistent in NVM) — already in production via `sendStatusUploadSet()`.

---

## Nodes observed on the bus

| Node ID | Role | Evidence |
|---------|------|----------|
| `0x01`  | T-Motor ESC | Emits NodeStatus (DataTypeId 341) periodically; source of all CUBECAN STAT1–4 broadcasts |
| `0x65`  | CloudLink configurator | Sends all DataTypeId 1000 commands and the CUBECAN `ENA` enable broadcast |

(Earlier revisions of this doc listed `0x02`, `0x42`, `0x45` as separate nodes
— that was wrong. Those values were the low byte of CUBECAN extended IDs
`0x10000002` / `0x10000042` / `0x100000C5`, mistakenly decoded as DroneCAN
node IDs.)

---

## DataTypeId map

| DataTypeId | Direction | Description |
|-----------|-----------|-------------|
| 341  | ESC → bus      | NodeStatus heartbeat (standard DroneCAN) |
| 999  | ESC → 0x65     | ESC response to GET/SET commands (multi-frame, ~3 frames) |
| 1000 | 0x65 → ESC     | CloudLink command to ESC (GET: 2 frames; SET: 3 + 10 + (optional) 8 frames) |
| 1001 | ESC → bus      | High-frequency ESC telemetry ~10 Hz (multi-frame) |
| 1131 | ESC → bus      | High-frequency ESC telemetry ~10 Hz — first 2 bytes contain motor temperature |

---

## Frame structure (DroneCAN recap)

All frames use 29-bit extended CAN IDs.

**Message frame** (bit 7 of CAN ID = 0):

```
[28:26] Priority (3 bits)
[25:8]  DataTypeId (18 bits, effective range 0–32767)
[7]     0 (message)
[6:0]   Source Node ID
```

**Service frame** (bit 7 of CAN ID = 1):

```
[28:26] Priority
[25:16] Service Type ID
[15]    1 = Request / 0 = Response
[14:8]  Destination Node ID
[7]     1 (service)
[6:0]   Source Node ID
```

**Tail byte** (last byte of every CAN frame payload):

```
[7]    Start-of-Frame
[6]    End-of-Frame
[5]    Toggle
[4:0]  Transfer ID
```

---

## Generic Instruction (DataTypeId 1000) — official protocol

Per `TMOTOR ESCs CAN Communication Manual V2.2.0` §3.2, DataTypeId 1000 carries
a "Generic Instruction" wrapper. The wrapper has a 6-byte header followed by a
variable-length payload, all CRC-protected by the standard DroneCAN multi-frame
mechanism.

### `CAN_APP_PROTO_HEAD` (6 bytes)

```c
#define CAN_APP_PROTO_MAGIC 0xabcd

typedef struct {
    uint16_t magic;    // Always 0xABCD (little-endian: CD AB)
    uint16_t msg_id;   // Internal protocol message ID
    uint16_t len;      // Length of the payload that follows
} CAN_APP_PROTO_HEAD;
```

### Documented internal `msg_id` values

| `msg_id` | Hex     | Name                       | Payload                           |
|---------:|---------|----------------------------|-----------------------------------|
| 4670     | `0x123E`| `ENA_ESC_STAT_REP_MSG_ID`  | `uint32_t enable` — 0=OFF, 1=ON   |
| 4669     | `0x123D`| `ARM_LED_CTRL_MSG_ID`      | LED control (8× ESCs)             |

### Enable Reporting — the test we want to run first

**`ENA_ESC_STAT_REP_T`** (4 bytes): `uint32_t enable` (0 turns reporting OFF,
1 turns reporting ON). When ON, the ESC begins emitting Status 1–5 (DataTypeId
1150–1154) at 10 Hz. Status 5 (1154) is the one we care about — it carries
motor temperature.

Total payload sent on DataTypeId 1000 = **6-byte header + 4-byte data = 10 bytes**.
With 2-byte multi-frame CRC prefix = 12 bytes → 2 CAN frames.

**Digital signature** (for the multi-frame CRC):
`0x1362ab78cd12f03aULL` (Generic Instruction ID 999/1000).

This is exactly what `TmotorCan::sendEnableReporting(bool enable)` already
implements at [TmotorCan.cpp:554](../src/Tmotor/TmotorCan.cpp). It is currently
defined but **not yet wired up** in `main.cpp`.

---

## Status 5 (DataTypeId 1154) — motor temperature frame

Single-frame, 7 bytes data + 1 tail byte. Reported at 10 Hz once Enable
Reporting is ON. Already parsed by `TmotorCan::handleEscStatus5()`.

```c
typedef struct {
    int16_t idc;        // Estimated busbar current (0.1 A units)
    int16_t cap_temp;   // Capacitor temperature (0.1 °C units)
    int16_t motor_temp; // Motor temperature (0.1 °C units)  ← what we want
    uint16_t resv: 8;   // Reserved
    uint16_t tail: 8;   // DroneCAN tail byte
} S_CAN_MSG_ESC_STAT5;
```

Signature for Status 1–5: `0x2362ab78cd12f03aULL`.

---

## CloudBoxOld "ESC Advanced Config" UI (observed)

Screenshot of the configurator window during the capture sessions. The
interesting toggle is **Status upload: Open / Close** — flipping it is what
generates the SET multi-frame transfers in the captures below.

| Section | Field              | Value observed       |
|---------|--------------------|----------------------|
| Left    | Throttle holding time | 0.25 s            |
| Left    | Dual throttle priority| PWM First         |
| Left    | CAN Baud Rate      | 1 M                  |
| Left    | Over Volt (V)      | 63                   |
| Left    | Under Volt (V)     | 20                   |
| Left    | PWM Width Max / Min| 1940 / 1100          |
| Right   | **Status upload**  | **Open / Close**     |
| Right   | Status protocol    | DRONECAN-S           |
| Right   | Status frequency   | 10 Hz                |
| Right   | ADRW               | 6653                 |

`DRONECAN-S` is selected for command/response (poll, parameter SET/GET).
Independently, the ESC also broadcasts CUBECAN status frames (fixed
`0x10000xxx` IDs) — the two protocols coexist on the same bus. See the
*CUBECAN ID map* section below.

---

## CUBECAN ID map (V2.2 §4)

The ESC broadcasts a parallel CUBECAN telemetry stream alongside DroneCAN.
Frames use fixed 29-bit extended IDs in the `0x10000000`–`0x10000146` range.
The IDs we see on this bus, decoded from the V2.2 spec:

| CAN ID         | Name                       | Direction | Purpose                                              |
|----------------|----------------------------|-----------|------------------------------------------------------|
| `0x10000000`   | `CUBECAN_ESC_CMD_ID`       | FC → ESC  | Throttle command (4 ESCs/frame, 10-bit cmd 0–1000)   |
| `0x10000001`+N | `CUBECAN_ESCN_STAT1_ID`    | ESC → FC  | Status 1: mode, throttle, RPM, MOS temp              |
| `0x10000041`+N | `CUBECAN_ESCN_STAT2_ID`    | ESC → FC  | Status 2: Vbus (0.1V), Irms (0.1A), Idq (0.1A)       |
| `0x10000081`+N | `CUBECAN_ESCN_STAT3_ID`    | ESC → FC  | Status 3: alg_err, alg_warn, vdq_duty                |
| `0x100000C1`   | `CUBECAN_ESC_LED_ID`       | FC → ESC  | LED control                                          |
| `0x100000C2`   | `CUBECAN_ESC_ENA_ID`       | FC → ESC  | Enable status reporting (sweeps node IDs, 4/frame)   |
| `0x100000C3`   | `CUBECAN_ANG_SET_ID`       | FC → ESC  | Angle set                                            |
| `0x100000C4`+N | `CUBECAN_ESCN_STAT4_ID`    | ESC → FC  | Status 4: Idc, cap_temp, motor_temp, resv (0.1°C)    |
| `0x10000104`   | `CUBECAN_ESC_QUERY_STATE`  | FC → ESC  | Query single ESC                                     |
| `0x10000106`   | `CUBECAN_OPE_ID`           | FC → ESC  | Read/Write parameters (incl. motor direction*)       |
| `0x10000107`+N | `CUBECAN_ESCN_ACK_ID`      | ESC → FC  | Response to OPE                                      |

`N` = ESC node ID (0–63). For our single ESC at node 1: STAT1 = `0x10000002`,
STAT2 = `0x10000042`, STAT3 = `0x10000082`, STAT4 = `0x100000C5`.

\* `CUBECAN_OPE_ID` with `cs = CS_BATCH_SET_MOTOR_DIR_REQ` (`0x12`) and
`data = 1`/`-1` is the **documented** direction-set channel, but the V2.2
spec says it requires ESC reboot to take effect. CloudLink instead uses an
**undocumented real-time path** via the proprietary DroneCAN msg_id `0x1247`
(see below) that takes effect without reboot.

---

## Configuration protocol (DataTypeId 1000) — CloudLink proprietary

All configuration commands originate from node `0x65` (CloudLink) as multi-frame
DroneCAN transfers on DataTypeId 1000. The ESC replies on DataTypeId 999.

The DroneCAN wrapper is the **same** `CAN_APP_PROTO_HEAD` as the official Generic
Instruction (magic `0xABCD`, then `msg_id`, then `len`). What changes is the
internal `msg_id` and the payload layout — none of these are documented in
either PDF.

### Internal `msg_id` values seen in captures

| `msg_id` | Hex      | Direction | Purpose                           |
|---------:|----------|-----------|-----------------------------------|
| 4667     | `0x123B` | 0x65 → ESC| Periodic poll (4-byte payload)    |
| 4668     | `0x123C` | ESC → 0x65| Response to 0x123B (8 bytes)      |
| 4675     | `0x1243` | 0x65 → ESC| Read register/parameter request   |
| 4676     | `0x1244` | ESC → 0x65| Response to 0x1243                |
| 4679     | `0x1247` | both      | **Parameter block read/write**    |
| 4683     | `0x124B` | 0x65 → ESC| Periodic poll (4-byte payload)    |
| 4684     | `0x124C` | ESC → 0x65| Response to 0x124B (12 bytes)     |

The earlier doc claim that "every transfer uses `msg_id = 0x1247`" was wrong —
that's only the SET/GET parameter command. The 9-frame periodic bursts
CloudLink sends every ~200 ms are `0x123B` + `0x124B` polls.

### SET command (clicking "Set" with "Status upload" toggled)

Three back-to-back multi-frame transfers, all on DataTypeId 1000 with
internal `msg_id = 0x1247`:

| Transfer | `len` | Frames | Inner data starts with     | What it carries |
|---------:|:-----:|:------:|----------------------------|-----------------|
| 1        | 0x0C  | 3      | `41 00 01 00 02 00 ...`    | Section 0x02 — likely "begin write" handshake |
| 2        | 0x38  | 10     | `41 00 01 00 04 00 ...`    | Section 0x04 — general ESC config (PWM range, voltage limits, etc.) |
| 3        | 0x2C  | 8      | `41 00 01 00 10 00 ...`    | Section 0x10 — **Status upload setting** |

All three transfers begin with the inner-data prefix `41 00 01 00 SS 00`
where `SS` is the section index. Status upload is section `0x10` (16).

**Captured Transfer 1 inner data (12 bytes, identical Close vs Open):**
```
41 00 01 00 02 00 04 00 00 00 00 00
```

**Captured Transfer 2 inner data (56 bytes, layout):**
```
41 00 01 00 04 00 30 00      ← section header (sec=0x04, body_len=0x30)
01 00 DD DD 4C 04 00 00      ← body[0..7] — DD DD = ROTATION DIRECTION (int16)
94 07 00 00 76 02 C8 00      ← body[8..15]  (94 07 = 1940 PWM max)
01 00 00 00 00 00 A0 0F      ← body[16..23] (A0 0F = 4000)
A0 0F 32 00 01 00 00 00      ← body[24..31] (32 00 = 50)
00 00 00 00 00 00 00 00      ← body[32..39]
00 00 00 00 00 00 00 00      ← body[40..47]
```

**Rotation direction is encoded at body bytes 2-3 (= inner offset [10..11]) as an
`int16_t` little-endian:**

| Bytes  | int16 LE | Direction |
|--------|---------:|-----------|
| `01 00`| `+1`     | **forward** |
| `FF FF`| `-1`     | **reverse** |

Per V2.2 §4.6.4 the protocol-level value is `1` for forward and `-1` for reverse.
Empirically tested: each click on the direction toggle in CloudLink emits
Transfer 1 + Transfer 2 with body[2..3] alternating between `01 00` and `FF FF`,
and **the motor changes direction immediately** — no reboot needed (despite the
manual claiming reboot is required for parameter writes via CUBECAN_OPE_ID).

Notable: `94 07` = 0x0794 = 1940 (PWM Width Max from the CloudLink screenshot);
`4C 04` = 0x044C = 1100 (PWM Width Min). The block carries the user's generic
ESC settings — **replicating Transfer 2 verbatim will overwrite custom config**.

**Captured Transfer 3 inner data (44 bytes, ONLY byte 28 differs):**
```
41 00 01 00 10 00 00 24 00 7B 00 00 00 02 00 00
00 00 00 00 00 40 42 0F 00 00 00 00 XX 00 01 00
00 00 00 00 00 00 00 00 00 FA 00 00 00
```
- `XX = 0x00` → Status upload **Close** (ESC stops emitting Status 1–5)
- `XX = 0x80` → Status upload **Open** (ESC emits Status 1–5 at 10 Hz)

Byte 28 of the inner data = byte 1 of CAN frame 6 of Transfer 3 (the second
non-CRC byte of that frame). The earlier doc said "byte 36 / first byte of
F6" — that was wrong on both counts.

### Notes on protocol behaviour

- **Persistence**: the Status upload setting is saved in the ESC's non-volatile
  memory. Power cycling does not reset it. Once CloudLink writes "Close",
  the ESC stays in Close until something writes "Open" again.
- **Path A vs Path B**: the official `Enable Reporting` (msg_id `0x123E`) only
  enables reporting at the *session* level. It works when the ESC's persistent
  Status upload is `Open`, but it cannot override a persistent `Close` —
  empirically verified by us. To reliably enable Status 5 from the firmware
  regardless of the ESC's saved state, Path B (this proprietary SET) is needed.
- **Source node**: captures show `0x65` (CloudLink) but the ESC does not appear
  to filter by source — sending from any node ID works.

---

## Path C — Real-time motor direction switch

Discovered by observing CloudLink's traffic while clicking the direction toggle
4 times in a row (`can5.log`, ms timestamps 36103 / 42532 / 48233 / 51574).
Each click emits the same Transfer 1 + Transfer 2 sequence as the Status upload
SET, but only **two bytes** of Transfer 2 change: the rotation direction at
inner offset `[10..11]`.

```c
// Sequence per click (single 8-byte CAN frame each line):

// Transfer 1 — section 0x02 begin write handshake (12-byte inner)
//   Tail bytes: SOF / toggle / EOF, transfer_id increments per click.
F1: <CRC_lo> <CRC_hi> CD AB 47 12 0C  | tail
F2: 00 41 00 01 00 02 00              | tail
F3: 04 00 00 00 00 00                 | tail (DLC=7)

// Transfer 2 — section 0x04 with rotation direction (56-byte inner)
F1:  <CRC_lo> <CRC_hi> CD AB 47 12 38 | tail
F2:  00 41 00 01 00 04 00             | tail   (section header)
F3:  30 00 01 00 DD DD 4C             | tail   ← DD DD = direction (int16 LE)
F4:  04 00 00 94 07 00 00             | tail
F5:  76 02 C8 00 01 00 00             | tail
F6:  00 00 00 A0 0F A0 0F             | tail
F7:  32 00 01 00 00 00 00             | tail
F8:  00 00 00 00 00 00 00             | tail
F9:  00 00 00 00 00 00 00             | tail
F10: 00                               | tail (DLC=2)
```

Direction value at body[2..3]:
- `01 00` (LE +1) → **forward**
- `FF FF` (LE -1) → **reverse**

CRC of Transfer 1: deterministic, always `0x2053` (= bytes `53 20`).
CRC of Transfer 2: depends on direction byte:
- forward: `0xCEDB` (`DB CE`)
- reverse: `0x6E9D` (`9D 6E`)

Both CRCs computed CRC16-CCITT seeded with the Generic Instruction signature
`0x1362ab78cd12f03aULL`, same as `sendEnableReporting` already uses.

ESC ACKs each Transfer on `0x0803E701` with `msg_id = 0x1247`.

**Caveat — same as Path B**: Transfer 2 carries the full general-config block
(PWM range, voltage limits, etc.) verbatim from the captured CloudLink traffic.
Replicating it overwrites whatever those settings are on the ESC. If the firmware
exposes a "switch direction" action, it should either:
- replay the captured Transfer 2 with only `body[2..3]` patched (assumes the
  user's PWM/voltage config matches the captured values), **or**
- first issue a GET to read the current section 0x04, modify only `body[2..3]`,
  and write it back (safer; protocol of the GET is not yet captured).

---

## Two paths to enable Status 5

### Path A — Official `Enable Reporting` (msg_id 0x123E)

Single 2-frame transfer on DataTypeId 1000:

```
Header:  CD AB 3E 12 04 00              (magic, msg_id=0x123E, len=4)
Data:    01 00 00 00                    (enable = 1)   or   00 00 00 00 (disable)
CRC:     CRC16-CCITT seeded with signature 0x1362ab78cd12f03aULL
```

**Test result:** `enable=1` works — Status 5 (1154) starts arriving at 10 Hz.
**`enable=0` is ignored** by the ESC. Worse, this command only takes effect
at the *session* level: if CloudLink has saved "Status upload: Close" in the
ESC's non-volatile memory, Path A cannot override it. Verified empirically.

### Path B — Replicate CloudLink's parameter SET (msg_id 0x1247)

Sends three multi-frame transfers verbatim from the captured CloudLink traffic:
Transfer 1, Transfer 2, Transfer 3. Transfer 3 has byte 28 of its inner data
patched to `0x80` (Open) or `0x00` (Close).

This writes the Status upload setting persistently in the ESC's non-volatile
memory, so it survives reboots and overrides any earlier CloudLink state.

**Caveat:** Transfer 2 carries general ESC config (PWM range, voltage limits)
and is replayed verbatim from the capture. If the user changes those settings
via CloudLink later, replaying our hardcoded Transfer 2 will revert them.
The user should configure the ESC fully in CloudLink first, then leave it
alone — our firmware only flips the Status upload bit.

---

## Capture environment

- **Bus speed:** 1 Mbps
- **Sniffer:** ESP32-C3 (TWAI_MODE_NORMAL — actively ACKs), GPIO2=TX, GPIO3=RX
- **Firmware:** `feature/can-monitor` branch
- **Serial baud:** 921600 (115200 saturates with full 1 Mbps traffic)
- **TWAI rx queue:** 256 frames (default 5 is too small for bursts)
- **Captures:** `can.log`, `can2.log`, `can3.log`, `can4.log`, `can5.log`
  (local, not committed). `can5.log` is the cleanest — smart filter dropping
  the 8 known periodic broadcasts plus per-frame `ms=` timestamps and TWAI
  drop counters, so the 4 direction-toggle SETs surface unambiguously.
