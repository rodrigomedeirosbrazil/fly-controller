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

## Configuration protocol (DataTypeId 1000) — CloudBoxOld proprietary

All configuration commands originate from node `0x65` (CloudBox) as multi-frame
DroneCAN transfers on DataTypeId 1000.  The ESC replies on DataTypeId 999.

This protocol uses a **different internal `msg_id` (`0x1247`)** than the
documented `Enable Reporting` (`0x123E`), and a different payload layout
(parameter blocks rather than a single `uint32_t enable` flag). It is not
covered by either PDF and was reconstructed from the captures alone.

### Magic header

Every transfer payload begins with a 7-byte header.  Bytes 2–3 are always
`CD AB` (little-endian magic `0xABCD` — same `CAN_APP_PROTO_MAGIC` as the
official protocol).  Bytes 4–5 are always `47 12` → internal `msg_id = 0x1247`
(not documented in either PDF; presumably an internal "set parameter block"
command).

```
[0]     sequence counter (increments per command)
[1]     sequence counter high byte
[2-3]   CD AB  (magic 0xABCD)
[4-5]   47 12  (protocol marker)
[6]     payload length byte (0x0C = GET, 0x38 = SET large block, 0x2C = SET param block)
```

### GET command

Sent as **two back-to-back transfers** on DataTypeId 1000:

**Transfer A** (3 frames, header length 0x0C):
```
E4 31 CD AB 47 12 0C | 00 41 00 01 00 0E 00 | 04 00 00 00 00 00 <tail>
```

**Transfer B** (3 frames, header length 0x0C):
```
53 20 CD AB 47 12 0C | 00 41 00 01 00 02 00 | 04 00 00 00 00 00 <tail>
```

The ESC responds on DataTypeId 999 with the current parameter block.

### SET command

Sent as **three back-to-back transfers** on DataTypeId 1000:

**Transfer 1** — identical to GET Transfer B above.

**Transfer 2** — large block (header length 0x38, ~8 frames):  
Contains general ESC parameters.  Content is identical whether enabling or
disabling "Status upload" — this block carries other settings.

**Transfer 3** — parameter block (header length 0x2C, ~7 frames):  
Contains the "Status upload" (temperature reporting) setting.

Frame 2 of Transfer 3 always contains:
```
00 41 00 01 00 10 00
```
This identifies **parameter index `0x10` (16)** = "Status upload".

**Byte 36 of the assembled Transfer 3 payload** (first byte of the 6th CAN frame):

| Value  | Meaning |
|--------|---------|
| `0x00` | Status upload: **Close** — ESC stops reporting temperature |
| `0x80` | Status upload: **Open** — ESC reports temperature at configured frequency |

---

## Two paths to enable Status 5

### Path A — Official `Enable Reporting` (try this first)

Already implemented at [TmotorCan.cpp:554](../src/Tmotor/TmotorCan.cpp). Send a
single 2-frame transfer on DataTypeId 1000:

```
Header:  CD AB 3E 12 04 00              (magic, msg_id=0x123E, len=4)
Data:    01 00 00 00                    (enable = 1)   or   00 00 00 00 (disable)
CRC:     CRC16-CCITT seeded with signature 0x1362ab78cd12f03aULL
```

If after sending this we start seeing periodic Status 5 frames (DataTypeId
1154, 8 bytes) on the bus, we're done — no need for Path B.

### Path B — Replicate CloudBoxOld (fallback)

To replicate "Status upload: Open/Close" the way CloudBoxOld does it:

1. Send **GET** (Transfer A + Transfer B) to confirm current state from the ESC
   response on DataTypeId 999.
2. Build **Transfer 3** from a captured reference payload and patch byte 36:
   - `0x00` → disable temperature reporting
   - `0x80` → enable temperature reporting
3. Send **Transfer 1 → Transfer 2 → Transfer 3** in sequence on DataTypeId 1000
   from source node `0x65` (or whichever node ID we assign to the controller).

**Important:** the sequence counter bytes [0–1] increment with each command pair.
Replaying a fixed sequence counter may be rejected by the ESC; increment it per
send or mirror the counter from a prior GET response.

---

## Capture environment

- **Bus speed:** 1 Mbps
- **Sniffer:** ESP32-C3 (TWAI_MODE_LISTEN_ONLY), GPIO2=TX, GPIO3=RX
- **Firmware:** `feature/can-monitor` branch
- **Captures:** `can.log`, `can2.log`, `can3.log` (local, not committed)
