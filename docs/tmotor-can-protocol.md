# T-Motor ESC — CAN Bus Protocol Analysis

Reverse-engineered from three live captures on a 1 Mbps DroneCAN bus between a
T-Motor ESC and the CloudBoxOld configurator app.  
Capture tool: ESP32-C3 in TWAI_MODE_LISTEN_ONLY (`feature/can-monitor`).

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

## Configuration protocol (DataTypeId 1000)

All configuration commands originate from node `0x65` (CloudBox) as multi-frame
DroneCAN transfers on DataTypeId 1000.  The ESC replies on DataTypeId 999.

### Magic header

Every transfer payload begins with a 7-byte header.  Bytes 2–3 are always
`CD AB` (little-endian magic `0xABCD`).  Bytes 4–5 are always `47 12`.

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

## Implementing the toggle

To replicate "Status upload: Open/Close" programmatically:

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

## Related ESC parameters (observed from screen captures)

From the CloudBoxOld UI during the capture sessions:

| Parameter | Value observed |
|-----------|---------------|
| Status upload | Open / Close (toggled during captures) |
| Status protocol | DRONECAN-S |
| Status frequency | 10 Hz |

Temperature data appears in the first 2 bytes of DataTypeId 1131 frames when
"Status upload: Open" is active.

---

## Capture environment

- **Bus speed:** 1 Mbps
- **Sniffer:** ESP32-C3 (TWAI_MODE_LISTEN_ONLY), GPIO2=TX, GPIO3=RX
- **Firmware:** `feature/can-monitor` branch
- **Captures:** `can.log`, `can2.log`, `can3.log` (local, not committed)
