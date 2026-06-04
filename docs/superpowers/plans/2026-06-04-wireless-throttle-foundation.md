# Wireless Throttle — Foundation Implementation Plan (Plan 1 of 3)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restructure the repo into a monorepo (`controller/` + `throttle/` + `shared/`), add the shared ESP-NOW protocol header with host-runnable tests, and remove the dead `WIFI_OFF` path while documenting the C3 Supermini TX-power workaround.

**Architecture:** The current single PlatformIO project (root `platformio.ini` + `src/`, `include/`, `lib/`, `test/`, `web/`, `diagram.json`) moves wholesale into `controller/`. A new `shared/RemoteLinkProtocol.h` defines the byte-layout of both ESP-NOW packets and a counter-dedup helper, included by both firmwares. CI is updated to build from `controller/`.

**Tech Stack:** PlatformIO, ESP32-C3 Arduino framework, Unity/native-host tests (mirroring `test/PowerTest.cpp`).

**Spec:** `docs/superpowers/specs/2026-06-04-wireless-throttle-espnow-design.md`

---

### Task 1: Monorepo restructure — move controller project into `controller/`

**Files:**
- Move: `platformio.ini`, `src/`, `include/`, `lib/`, `test/`, `web/`, `diagram.json` → `controller/`
- Modify: `.github/workflows/build-and-release.yml`
- Modify: `CLAUDE.md` (root), `src/CLAUDE.md` build-path references
- Modify: `.gitignore` (if it references `.pio/` / `src/` at root)

- [ ] **Step 1: Move the controller project files with git mv**

```bash
cd /Users/rodrigo/dev/fly-controller
mkdir -p controller
git mv platformio.ini src include lib test web diagram.json controller/
git status
```
Expected: all listed paths shown as renamed into `controller/`.

- [ ] **Step 2: Verify the controller still builds from its new location**

```bash
cd /Users/rodrigo/dev/fly-controller/controller && ~/.platformio/penv/bin/pio run -e lolin_c3_mini_hobbywing
```
Expected: `SUCCESS`. (PlatformIO resolves `src/`, `lib/`, `test/` relative to the dir containing `platformio.ini`, so no config change is needed for the move itself.)

- [ ] **Step 3: Update CI workflow to build from `controller/`**

In `.github/workflows/build-and-release.yml`:
- Change the `Set version` step's `src/Version.h` path to `controller/src/Version.h` (all four `echo ... >> src/Version.h` lines → `controller/src/Version.h`).
- Add `working-directory: controller` to the three `pio run ...` build steps (Hobbywing, T-Motor, XAG).
- In `Prepare release artifacts`, change the three source paths `.pio/build/...` to `controller/.pio/build/...`.

Concretely, the build steps become:

```yaml
      - name: Build Hobbywing
        working-directory: controller
        run: pio run -e lolin_c3_mini_hobbywing

      - name: Build T-Motor
        working-directory: controller
        run: pio run -e lolin_c3_mini_tmotor

      - name: Build XAG
        working-directory: controller
        run: pio run -e lolin_c3_mini_xag
```

And the artifact copies become:

```bash
          cp controller/.pio/build/lolin_c3_mini_hobbywing/firmware.bin "release/firmware-hobbywing-${TAG}.bin"
          cp controller/.pio/build/lolin_c3_mini_tmotor/firmware.bin "release/firmware-tmotor-${TAG}.bin"
          cp controller/.pio/build/lolin_c3_mini_xag/firmware.bin "release/firmware-xag-${TAG}.bin"
```

- [ ] **Step 4: Update build-path references in docs**

- Root `CLAUDE.md`: prefix the build commands with the new dir, e.g. `~/.platformio/penv/bin/pio run -e lolin_c3_mini_hobbywing` → run from `controller/`. Add a one-line note that the controller firmware lives in `controller/`.
- `controller/src/CLAUDE.md`: update the `Build command:` line to note it runs from `controller/`.

- [ ] **Step 5: Verify all three build targets + tests build from the new layout**

```bash
cd /Users/rodrigo/dev/fly-controller/controller && \
  ~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor && \
  ~/.platformio/penv/bin/pio run -e lolin_c3_mini_xag && \
  ~/.platformio/penv/bin/pio test -e lolin_c3_mini --filter PowerTest
```
Expected: both builds `SUCCESS`; PowerTest passes.

- [ ] **Step 6: Commit**

```bash
cd /Users/rodrigo/dev/fly-controller
git add -A
git commit -m "refactor: move controller firmware into controller/ for monorepo

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 2: Shared ESP-NOW protocol header + host test

**Files:**
- Create: `shared/RemoteLinkProtocol.h`
- Create: `controller/test/RemoteLinkProtocolTest.cpp`

- [ ] **Step 1: Write the failing host test**

Create `controller/test/RemoteLinkProtocolTest.cpp`. It compiles the header on the host and asserts struct layout + the dedup helper. (Mirrors the host-only style of `PowerTest.cpp`.)

```cpp
#include <cassert>
#include <cstdint>
#include <iostream>
#include "../../shared/RemoteLinkProtocol.h"
using namespace std;

void test_packet_sizes_are_fixed() {
    // Layout must stay byte-identical across both firmwares.
    assert(sizeof(ThrottleToControllerPacket) == 4);
    assert(sizeof(ControllerToThrottlePacket) == 4);
}

void test_button_event_enum_values() {
    assert(RemoteButtonEvent::None == 0);
    assert(RemoteButtonEvent::ShortClick == 1);
    assert(RemoteButtonEvent::LongPress == 2);
}

void test_counter_advances_only_on_change() {
    uint8_t lastSeen = 5;
    assert(remoteLinkCounterAdvanced(5, lastSeen) == false); // same -> no action
    assert(remoteLinkCounterAdvanced(6, lastSeen) == true);  // changed -> act
    assert(lastSeen == 6);                                   // updated
    assert(remoteLinkCounterAdvanced(6, lastSeen) == false); // same again
}

void test_counter_wraps_around() {
    uint8_t lastSeen = 255;
    assert(remoteLinkCounterAdvanced(0, lastSeen) == true);  // 255 -> 0 is a change
    assert(lastSeen == 0);
}

int main() {
    test_packet_sizes_are_fixed();
    test_button_event_enum_values();
    test_counter_advances_only_on_change();
    test_counter_wraps_around();
    cout << "RemoteLinkProtocolTest: all passed" << endl;
    return 0;
}
```

- [ ] **Step 2: Run the test to verify it fails (header missing)**

```bash
cd /Users/rodrigo/dev/fly-controller && \
  c++ -std=c++17 -o /tmp/rlptest controller/test/RemoteLinkProtocolTest.cpp 2>&1 | head
```
Expected: FAIL — `shared/RemoteLinkProtocol.h: No such file or directory`.

- [ ] **Step 3: Write the protocol header**

Create `shared/RemoteLinkProtocol.h`:

```cpp
#ifndef REMOTE_LINK_PROTOCOL_H
#define REMOTE_LINK_PROTOCOL_H

#include <stdint.h>

// Shared ESP-NOW wire contract between the controller and the remote throttle.
// MUST stay byte-identical on both sides — both firmwares include this header.

// Fixed radio channel: ESP-NOW peers and the controller softAP must share a
// channel on the ESP32-C3's single radio. Matches the softAP default.
#define REMOTE_LINK_CHANNEL 1

// Discrete button events produced by the remote (AceButton on the remote side).
namespace RemoteButtonEvent {
    enum Type : uint8_t {
        None = 0,
        ShortClick = 1, // arm / disarm
        LongPress = 2,  // start calibration
    };
}

// Beep commands the controller asks the remote to play. Mapped to named
// Buzzer patterns on the remote side.
namespace RemoteBeep {
    enum Type : uint8_t {
        None = 0,
        SystemStart = 1,
        CalibrationStep = 2,
        Armed = 3,
        Disarmed = 4,
        Alert = 5,
    };
}

#pragma pack(push, 1)

// Remote -> Controller, sent ~50 Hz.
struct ThrottleToControllerPacket {
    uint16_t hallRaw;            // raw Hall ADC reading
    uint8_t  buttonEventCounter; // monotonic; +1 per discrete button event
    uint8_t  buttonEventType;    // RemoteButtonEvent::Type
};

// Controller -> Remote, heartbeat ~5 Hz + on-change.
struct ControllerToThrottlePacket {
    uint8_t armed;              // 1 = armed (red LED), 0 = disarmed (green LED)
    uint8_t calibrating;        // 1 = controller is in calibration sweep
    uint8_t beepCommandCounter; // monotonic; +1 per beep request
    uint8_t beepCommand;        // RemoteBeep::Type
};

#pragma pack(pop)

// Counter-based dedup shared by both sides: act on a received event only when
// its monotonic counter differs from the last one seen. Robust against the
// 50 Hz repetition and packet loss without needing ACKs. Updates `lastSeen`.
inline bool remoteLinkCounterAdvanced(uint8_t incoming, uint8_t &lastSeen) {
    if (incoming != lastSeen) {
        lastSeen = incoming;
        return true;
    }
    return false;
}

#endif // REMOTE_LINK_PROTOCOL_H
```

- [ ] **Step 4: Run the test to verify it passes**

```bash
cd /Users/rodrigo/dev/fly-controller && \
  c++ -std=c++17 -o /tmp/rlptest controller/test/RemoteLinkProtocolTest.cpp && /tmp/rlptest
```
Expected: `RemoteLinkProtocolTest: all passed`.

- [ ] **Step 5: Commit**

```bash
cd /Users/rodrigo/dev/fly-controller
git add shared/RemoteLinkProtocol.h controller/test/RemoteLinkProtocolTest.cpp
git commit -m "feat: add shared ESP-NOW protocol header with host tests

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 3: Remove dead `WIFI_OFF` path + document TX-power workaround

**Files:**
- Modify: `controller/src/WebServer/ControllerWebServer.cpp` (remove `stop()` body / declaration usage; document `setTxPower`)
- Modify: `controller/src/WebServer/ControllerWebServer.h` (remove `stop()` declaration)
- Modify: root `CLAUDE.md` (document TX-power rationale)

- [ ] **Step 1: Confirm `stop()` has no callers (guards the deletion)**

```bash
cd /Users/rodrigo/dev/fly-controller && grep -rn "webServer.stop\|webServer->stop\|\.stop()" controller/src | grep -i web
```
Expected: no matches referencing the web server's `stop()` (only unrelated `.stop()` like `buzzer.stop()` / `dnsServer.stop()` may appear). If a caller exists, STOP and revisit — the spec assumed dead code.

- [ ] **Step 2: Remove the `stop()` method**

In `controller/src/WebServer/ControllerWebServer.cpp`, delete the entire `ControllerWebServer::stop()` function (the block starting `void ControllerWebServer::stop() {` through its closing `}`, around lines 916–924).

In `controller/src/WebServer/ControllerWebServer.h`, delete the line `void stop();`.

- [ ] **Step 3: Document the TX-power workaround inline**

In `controller/src/WebServer/ControllerWebServer.cpp`, replace the `WiFi.setTxPower(WIFI_POWER_8_5dBm);` line with the same call preceded by:

```cpp
    // ESP32-C3 Supermini transmits unstably at full power; 8.5 dBm fixes it
    // (commit f06aa0d). ESP-NOW shares this radio/TX power — fine at close range.
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
```

- [ ] **Step 4: Document TX power + always-on WiFi in root CLAUDE.md**

In root `CLAUDE.md`, under the Web Portal section, add:

```markdown
WiFi is enabled at boot and stays on for the whole session (ESP-NOW shares the radio and must not be torn down). TX power is pinned to 8.5 dBm — the ESP32-C3 Supermini is unstable at full power (commit f06aa0d).
```

- [ ] **Step 5: Verify the controller still builds**

```bash
cd /Users/rodrigo/dev/fly-controller/controller && ~/.platformio/penv/bin/pio run -e lolin_c3_mini_hobbywing
```
Expected: `SUCCESS`.

- [ ] **Step 6: Commit**

```bash
cd /Users/rodrigo/dev/fly-controller
git add -A
git commit -m "refactor: remove dead WIFI_OFF path, document C3 TX-power workaround

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Self-Review

**Spec coverage (Foundation slice):**
- Monorepo `controller/`/`throttle/`/`shared/` → Task 1 creates `controller/` + `shared/`; `throttle/` is created in Plan 2. ✓
- `shared/RemoteLinkProtocol.h` packet structs + enums + dedup → Task 2. ✓
- Fixed channel constant → Task 2 (`REMOTE_LINK_CHANNEL`). ✓
- Remove `WIFI_OFF`/`stop()` dead code → Task 3. ✓
- Document `setTxPower(8.5 dBm)` rationale inline + CLAUDE.md → Task 3. ✓
- Failsafe, LED state machine, pairing, ReadFn wiring, web UI → **deferred to Plans 2 & 3** (controller integration + remote firmware). Not gaps.

**Placeholder scan:** none — all steps contain concrete code/commands.

**Type consistency:** `ThrottleToControllerPacket` / `ControllerToThrottlePacket`, `RemoteButtonEvent::Type`, `RemoteBeep::Type`, `remoteLinkCounterAdvanced(uint8_t, uint8_t&)`, `REMOTE_LINK_CHANNEL` — names used identically in header and test.

## Follow-on plans
- **Plan 2 — Remote throttle firmware** (`throttle/`): Hall `ReadFn`, AceButton events, ESP-NOW TX/RX, LED state machine, buzzer, pairing broadcast, remote-side link-loss failsafe.
- **Plan 3 — Controller integration**: `RemoteLink/` component, `Settings` (source + paired MAC), wireless `ReadFn` wiring into `Throttle`, button-event → `handleButtonEvent()`, controller-side hybrid failsafe (500 ms ramp / 3 s disarm), beep forwarding, web portal source selector + pairing control.
