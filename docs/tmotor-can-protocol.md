# T-Motor ESC — CAN Bus Protocol Analysis

Two complementary sources of truth:

1. **Official documentation** — `TMOTOR ESCs CAN Communication Manual V2.2.0.pdf`
   and `t-motor-uavcan-2.2.pdf`. Defines the DRONECAN-S protocol and the
   `Enable Reporting` Generic Instruction (msg_id 4670) — the simple way to
   turn ESC status reports on/off.
2. **Live captures** — three captures on a 1 Mbps DroneCAN bus between a
   T-Motor ESC and the CloudBoxOld configurator app, taken with an ESP32-C3 in
   `TWAI_MODE_LISTEN_ONLY` (`feature/can-monitor`). Show that CloudBoxOld does
   **not** use the simple Enable Reporting — it uses a proprietary parameter
   block protocol on the same DataTypeId 1000 (different internal `msg_id`).

These two paths likely produce the same end result ("Status upload: Open" →
ESC starts emitting Status 5 / DataTypeId 1154 with motor temperature). The
official path is ~50 lines of code; the CloudBoxOld path is ~17 multi-frames
across 3 multi-frame transfers.

---

## Nodes observed on the bus

| Node ID | Role | Evidence |
|---------|------|----------|
| `0x01`  | T-Motor ESC | Emits NodeStatus (DataTypeId 341) periodically |
| `0x65`  | CloudBox configurator | Sends all DataTypeId 1000 commands |
| `0x02`, `0x42`, `0x45` | Other devices | Periodic status frames only |

---

## DataTypeId map

| DataTypeId | Direction | Description |
|-----------|-----------|-------------|
| 341  | ESC → bus      | NodeStatus heartbeat (standard DroneCAN) |
| 999  | ESC → 0x65     | ESC response to GET/SET commands (multi-frame, ~3 frames) |
| 1000 | 0x65 → ESC     | CloudBox command to ESC (GET: 2 frames; SET: 8+ frames) |
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

`DRONECAN-S` confirms the bus is using the DroneCAN protocol described in
§3 of the manual (the alternative being CUBECAN, §4, which uses fixed
`0x10000xxx` extended IDs and is not in use here).

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

**Captured Transfer 2 inner data (56 bytes, identical Close vs Open):**
```
41 00 01 00 04 00 30 00 01 00 01 00 4C 04 00 00
94 07 00 00 76 02 C8 00 01 00 00 00 00 00 A0 0F
A0 0F 32 00 01 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
```
Notable: `94 07` = 0x0794 = 1940 (PWM Width Max from the CloudLink screenshot);
`4C 04` = 0x044C = 1100 (PWM Width Min). So this block carries the user's
generic ESC settings — replicating it verbatim will overwrite custom config.

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
- **Sniffer:** ESP32-C3 (TWAI_MODE_LISTEN_ONLY), GPIO2=TX, GPIO3=RX
- **Firmware:** `feature/can-monitor` branch
- **Captures:** `can.log`, `can2.log`, `can3.log` (local, not committed)
