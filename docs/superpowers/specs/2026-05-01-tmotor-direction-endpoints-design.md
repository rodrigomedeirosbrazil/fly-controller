# Design: T-Motor Real-Time Direction Change Endpoints

**Branch:** `feature/tmotor-direction-change`  
**Date:** 2026-05-01

## Summary

Add two `GET` HTTP endpoints that trigger a real-time motor direction switch on
the T-Motor ESC using the proprietary `msg_id 0x1247` CAN protocol path
(documented in `docs/tmotor-can-protocol.md`, Path C). Remove the existing
POST reporting-enable/disable endpoints (lines 495–520) from
`ControllerWebServer.cpp` as part of this cleanup.

## Endpoints

```
GET /api/tmotor/direction/forward?pin=XXXX
GET /api/tmotor/direction/reverse?pin=XXXX
```

- PIN validated via query parameter `pin` against `settings.getConfigPin()`.
- Both endpoints guarded by `#if IS_TMOTOR`.
- Response on success: `{"ok":true,"direction":"forward"}` / `{"ok":true,"direction":"reverse"}`
- Response on ESC not detected: `503 {"ok":false,"error":"ESC not detected on bus"}`
- Response on invalid PIN: `403 text/plain "PIN inválido"`

**Security note:** PIN in query param is acceptable here — the device is a
Wi-Fi AP with no upstream proxy or server-side logging.

## Changes

### 1. `src/Tmotor/TmotorCan.h`

Add public method:

```cpp
void sendDirectionSet(bool forward);
```

### 2. `src/Tmotor/TmotorCan.cpp`

Add `sendDirectionSet(bool forward)`:

- Sends Transfer 1 (`T1_INNER`, section `0x02`, 12 bytes) — begin write handshake.
- Sends Transfer 2 (section `0x04`, 56 bytes) with `inner[10..11]` patched:
  - forward → `0x01 0x00`
  - reverse → `0xFF 0xFF`

To avoid duplicating the `T1_INNER` and `T2_INNER` byte arrays that
`sendStatusUploadSet` already defines, promote them to `static const` at file
scope (or inside a shared helper). Only `inner[10..11]` of `T2_INNER` changes
between `sendStatusUploadSet` and `sendDirectionSet`.

Transfer 3 (section `0x10`, status upload toggle) is **not** sent here — direction
change does not require it.

### 3. `src/WebServer/ControllerWebServer.cpp`

- **Remove** lines 495–520 (the `#if IS_TMOTOR` block with
  `/api/tmotor/reporting/enable` and `/api/tmotor/reporting/disable` POST
  handlers).
- **Add** two `GET` handlers inside a new `#if IS_TMOTOR` block:

```cpp
server.on("/api/tmotor/direction/forward", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("pin") || request->getParam("pin")->value() != settings.getConfigPin()) {
        request->send(403, "text/plain", "PIN inválido"); return;
    }
    if (canbus.getEscNodeId() == 0) {
        request->send(503, "application/json", "{\"ok\":false,\"error\":\"ESC not detected on bus\"}"); return;
    }
    tmotorCan.sendDirectionSet(true);
    request->send(200, "application/json", "{\"ok\":true,\"direction\":\"forward\"}");
});

server.on("/api/tmotor/direction/reverse", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("pin") || request->getParam("pin")->value() != settings.getConfigPin()) {
        request->send(403, "text/plain", "PIN inválido"); return;
    }
    if (canbus.getEscNodeId() == 0) {
        request->send(503, "application/json", "{\"ok\":false,\"error\":\"ESC not detected on bus\"}"); return;
    }
    tmotorCan.sendDirectionSet(false);
    request->send(200, "application/json", "{\"ok\":true,\"direction\":\"reverse\"}");
});
```

## Files Touched

| File | Change |
|------|--------|
| `src/Tmotor/TmotorCan.h` | Add `sendDirectionSet` declaration |
| `src/Tmotor/TmotorCan.cpp` | Add `sendDirectionSet` implementation; promote T1/T2 arrays to shared scope |
| `src/WebServer/ControllerWebServer.cpp` | Remove lines 495–520; add 2 GET direction endpoints |

## Out of Scope

- Reading current direction from ESC before writing (GET of section `0x04` — protocol not yet captured).
- Web UI controls for direction toggle.
- Updating `MANUAL-INTERFACE-WEB.md` (doc update is a separate task).
