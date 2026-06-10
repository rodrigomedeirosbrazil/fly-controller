# Wireless Throttle — Remote Firmware Implementation Plan (Plan 2 of 3)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the standalone remote-throttle firmware in `throttle/`: it reads a Hall sensor and a push button, transmits them over ESP-NOW, drives two status LEDs and a buzzer from received controller state, and broadcasts for pairing.

**Architecture:** A second PlatformIO project at `throttle/` targeting the same `lolin_c3_mini` (ESP32-C3). Pure decision logic (LED patterns, link-loss detection, button-event counter) lives in a host-testable `RemoteLogic` module with no Arduino dependencies. Arduino/ESP-NOW glue lives in `main.cpp`. The self-contained `Buzzer` class is reused from `controller/src/Buzzer` via `lib_extra_dirs`; the wire contract comes from `shared/RemoteLinkProtocol.h`.

**Tech Stack:** PlatformIO, ESP32-C3 Arduino framework, ESP-NOW (`esp_now.h`/`esp_wifi.h`), AceButton, ESP32 `Preferences` (NVS), Unity/native-host tests.

**Spec:** `docs/superpowers/specs/2026-06-04-wireless-throttle-espnow-design.md`

**Remote pinout (approved):**

| GPIO | Function |
|---|---|
| 0 | Hall sensor (analogRead, ADC1_CH0) |
| 5 | Push button (INPUT_PULLUP) |
| 6 | Buzzer (LEDC PWM) |
| 7 | Red LED (armed) |
| 10 | Green LED (disarmed) |

---

### Task 1: `throttle/` project scaffold that builds

**Files:**
- Create: `throttle/platformio.ini`
- Create: `throttle/src/main.cpp`
- Create: `throttle/src/RemotePins.h`

- [ ] **Step 1: Create `throttle/platformio.ini`**

```ini
; Remote wireless-throttle firmware (ESP32-C3 Supermini).
; Reuses the self-contained Buzzer class from the controller via lib_extra_dirs,
; and the ESP-NOW wire contract from ../shared/RemoteLinkProtocol.h.
[platformio]
default_envs = remote_throttle

[env:remote_throttle]
platform = espressif32@6.12.0
board = lolin_c3_mini
framework = arduino
monitor_speed = 115200
build_flags =
	-Wall
	-Wextra
	-Wno-unused-parameter
	-I ../shared
lib_extra_dirs =
	../controller/src
lib_deps =
	bxparks/AceButton@1.10.1
```

> `lib_extra_dirs = ../controller/src` exposes each controller component folder as
> a candidate library; PlatformIO's LDF only compiles the ones actually `#include`d
> (here just `Buzzer`). No controller files are modified.

- [ ] **Step 2: Create `throttle/src/RemotePins.h`**

```cpp
#ifndef REMOTE_PINS_H
#define REMOTE_PINS_H

// ESP32-C3 Supermini pin assignments for the remote throttle.
#define HALL_PIN        0   // ADC1_CH0, analogRead
#define BUTTON_PIN      5   // INPUT_PULLUP
#define BUZZER_PIN      6   // LEDC PWM
#define LED_RED_PIN     7   // armed
#define LED_GREEN_PIN   10  // disarmed

#endif // REMOTE_PINS_H
```

- [ ] **Step 3: Create a minimal `throttle/src/main.cpp` that builds and links Buzzer + protocol**

```cpp
#include <Arduino.h>
#include <Buzzer.h>
#include "RemoteLinkProtocol.h"
#include "RemotePins.h"

Buzzer buzzer(BUZZER_PIN);

void setup() {
    Serial.begin(115200);
    pinMode(LED_RED_PIN, OUTPUT);
    pinMode(LED_GREEN_PIN, OUTPUT);
    buzzer.setup();
    buzzer.beepSystemStart();
}

void loop() {
    buzzer.handle();
}
```

- [ ] **Step 4: Verify the remote firmware builds**

```bash
cd /Users/rodrigo/dev/fly-controller/throttle && ~/.platformio/penv/bin/pio run -e remote_throttle
```
Expected: `SUCCESS`. (Confirms toolchain, Buzzer reuse via `lib_extra_dirs`, and `-I ../shared` protocol include all resolve. Run from the worktree's `throttle/` in this session.)

- [ ] **Step 5: Commit**

```bash
git add throttle/platformio.ini throttle/src/main.cpp throttle/src/RemotePins.h
git commit -m "feat(throttle): scaffold remote firmware project (builds, reuses Buzzer)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 2: `RemoteLogic` — host-testable decision logic

**Files:**
- Create: `throttle/src/RemoteLogic/RemoteLogic.h`
- Create: `throttle/test/RemoteLogicTest.cpp`

This module is pure C++ (no Arduino) so it compiles and runs on the host, mirroring `controller/test/PowerTest.cpp`.

- [ ] **Step 1: Write the failing host test**

Create `throttle/test/RemoteLogicTest.cpp`:

```cpp
#include <cassert>
#include <cstdint>
#include <iostream>
#include "../src/RemoteLogic/RemoteLogic.h"
using namespace std;

void test_led_pattern_priority() {
    // Pairing mode wins over everything.
    assert(pickLedPattern(/*paired*/true, /*pairing*/true, /*linkLost*/true, /*armed*/true)
           == RemoteLedPattern::PairingAlternate);
    // Not paired -> unpaired blink.
    assert(pickLedPattern(false, false, false, false) == RemoteLedPattern::UnpairedBlink);
    // Paired but link lost.
    assert(pickLedPattern(true, false, true, true) == RemoteLedPattern::LinkLostBlink);
    // Paired, linked, armed/disarmed solid.
    assert(pickLedPattern(true, false, false, true) == RemoteLedPattern::ArmedSolid);
    assert(pickLedPattern(true, false, false, false) == RemoteLedPattern::DisarmedSolid);
}

void test_led_render_solid() {
    LedOutput armed = renderLeds(RemoteLedPattern::ArmedSolid, 0);
    assert(armed.red == true && armed.green == false);
    LedOutput disarmed = renderLeds(RemoteLedPattern::DisarmedSolid, 12345);
    assert(disarmed.red == false && disarmed.green == true);
}

void test_led_render_blink_phases() {
    // At t=0 the blink phase is "on"; after one period it flips.
    LedOutput unpairedOn = renderLeds(RemoteLedPattern::UnpairedBlink, 0);
    LedOutput unpairedOff = renderLeds(RemoteLedPattern::UnpairedBlink, REMOTE_BLINK_PERIOD_MS);
    assert(unpairedOn.red != unpairedOff.red);
    assert(unpairedOn.green == false && unpairedOff.green == false);

    // Link-lost blinks both together (red == green).
    LedOutput linkLost = renderLeds(RemoteLedPattern::LinkLostBlink, 0);
    assert(linkLost.red == linkLost.green);

    // Pairing alternates (red != green).
    LedOutput pairing = renderLeds(RemoteLedPattern::PairingAlternate, 0);
    assert(pairing.red != pairing.green);
}

void test_link_lost_detection() {
    // No loss within the timeout window.
    assert(isLinkLost(/*lastRxMs*/1000, /*nowMs*/1000 + REMOTE_LINK_TIMEOUT_MS - 1) == false);
    // Loss once the timeout is exceeded.
    assert(isLinkLost(1000, 1000 + REMOTE_LINK_TIMEOUT_MS + 1) == true);
}

void test_button_event_counter_increments_and_wraps() {
    RemoteButtonState s; // counter starts at 0
    assert(s.counter == 0);
    recordButtonEvent(s, RemoteButtonEvent::ShortClick);
    assert(s.counter == 1 && s.type == RemoteButtonEvent::ShortClick);
    s.counter = 255;
    recordButtonEvent(s, RemoteButtonEvent::LongPress);
    assert(s.counter == 0 && s.type == RemoteButtonEvent::LongPress); // wraps
}

int main() {
    test_led_pattern_priority();
    test_led_render_solid();
    test_led_render_blink_phases();
    test_link_lost_detection();
    test_button_event_counter_increments_and_wraps();
    cout << "RemoteLogicTest: all passed" << endl;
    return 0;
}
```

- [ ] **Step 2: Run the test to verify it fails (module missing)**

```bash
cd /Users/rodrigo/dev/fly-controller && \
  c++ -std=c++17 -I shared -o /tmp/rltest throttle/test/RemoteLogicTest.cpp 2>&1 | head
```
Expected: FAIL — `RemoteLogic.h` file not found.

- [ ] **Step 3: Write `throttle/src/RemoteLogic/RemoteLogic.h`**

```cpp
#ifndef REMOTE_LOGIC_H
#define REMOTE_LOGIC_H

#include <stdint.h>
#include "RemoteLinkProtocol.h"

// Tunables (shared by firmware and tests).
#define REMOTE_BLINK_PERIOD_MS 250  // half-cycle of every blink pattern
#define REMOTE_LINK_TIMEOUT_MS 1000 // no controller packet for this long = link lost

// LED display patterns, highest priority first (see pickLedPattern).
enum class RemoteLedPattern {
    PairingAlternate, // red <-> green alternating
    UnpairedBlink,    // red blinking, green off
    LinkLostBlink,    // red + green blinking together
    ArmedSolid,       // red solid
    DisarmedSolid,    // green solid
};

struct LedOutput {
    bool red;
    bool green;
};

// Holds the monotonic button-event counter sent to the controller.
struct RemoteButtonState {
    uint8_t counter = 0;
    uint8_t type = RemoteButtonEvent::None;
};

// Choose the LED pattern from the current remote state. Priority:
// pairing > unpaired > link-lost > armed/disarmed.
inline RemoteLedPattern pickLedPattern(bool paired, bool pairingMode, bool linkLost, bool armed) {
    if (pairingMode) return RemoteLedPattern::PairingAlternate;
    if (!paired)     return RemoteLedPattern::UnpairedBlink;
    if (linkLost)    return RemoteLedPattern::LinkLostBlink;
    return armed ? RemoteLedPattern::ArmedSolid : RemoteLedPattern::DisarmedSolid;
}

// Render a pattern to concrete LED on/off states at time nowMs.
inline LedOutput renderLeds(RemoteLedPattern pattern, uint32_t nowMs) {
    bool phase = (nowMs / REMOTE_BLINK_PERIOD_MS) % 2; // toggles every period
    switch (pattern) {
        case RemoteLedPattern::ArmedSolid:      return {true, false};
        case RemoteLedPattern::DisarmedSolid:   return {false, true};
        case RemoteLedPattern::UnpairedBlink:   return {phase, false};
        case RemoteLedPattern::LinkLostBlink:   return {phase, phase};
        case RemoteLedPattern::PairingAlternate:return {phase, !phase};
    }
    return {false, false};
}

// True once no controller packet has arrived for REMOTE_LINK_TIMEOUT_MS.
inline bool isLinkLost(uint32_t lastRxMs, uint32_t nowMs) {
    return (nowMs - lastRxMs) > REMOTE_LINK_TIMEOUT_MS;
}

// Record a discrete button event: bump the monotonic counter (wraps) and store type.
inline void recordButtonEvent(RemoteButtonState &state, uint8_t eventType) {
    state.counter++; // uint8_t wraps 255 -> 0 naturally
    state.type = eventType;
}

#endif // REMOTE_LOGIC_H
```

- [ ] **Step 4: Run the test to verify it passes**

```bash
cd /Users/rodrigo/dev/fly-controller && \
  c++ -std=c++17 -I shared -o /tmp/rltest throttle/test/RemoteLogicTest.cpp && /tmp/rltest
```
Expected: `RemoteLogicTest: all passed`.

- [ ] **Step 5: Commit**

```bash
git add throttle/src/RemoteLogic/RemoteLogic.h throttle/test/RemoteLogicTest.cpp
git commit -m "feat(throttle): host-tested LED/link/button decision logic

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 3: ESP-NOW transport + full `main.cpp` integration

**Files:**
- Create: `throttle/src/RemoteEspNow/RemoteEspNow.h`
- Create: `throttle/src/RemoteEspNow/RemoteEspNow.cpp`
- Modify: `throttle/src/main.cpp`

`RemoteEspNow` wraps ESP-NOW init, peer management, send, and receive-callback bookkeeping. Pairing (saving/learning the controller MAC) is added in Task 4; for now it sends to the broadcast address and stores the last received controller packet + RX timestamp.

- [ ] **Step 1: Write `throttle/src/RemoteEspNow/RemoteEspNow.h`**

```cpp
#ifndef REMOTE_ESP_NOW_H
#define REMOTE_ESP_NOW_H

#include <stdint.h>
#include "RemoteLinkProtocol.h"

class RemoteEspNow {
  public:
    void setup();                                   // init WiFi STA + ESP-NOW on REMOTE_LINK_CHANNEL
    void sendThrottle(const ThrottleToControllerPacket &pkt); // unicast to peer (or broadcast if unpaired)
    bool hasState() const { return hasState_; }     // received at least one controller packet
    ControllerToThrottlePacket state() const { return state_; }
    uint32_t lastRxMs() const { return lastRxMs_; }
    void setPeer(const uint8_t mac[6]);             // set/replace the controller peer
    void setBroadcastPeer();                        // peer = ff:ff:ff:ff:ff:ff (unpaired/pairing)

    // Called from the static ESP-NOW recv trampoline.
    void onReceive(const uint8_t *senderMac, const uint8_t *data, int len);

  private:
    ControllerToThrottlePacket state_{};
    volatile bool hasState_ = false;
    volatile uint32_t lastRxMs_ = 0;
    uint8_t peerMac_[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t lastSenderMac_[6] = {0};

  public:
    const uint8_t *lastSenderMac() const { return lastSenderMac_; }
};

extern RemoteEspNow remoteEspNow;

#endif // REMOTE_ESP_NOW_H
```

- [ ] **Step 2: Write `throttle/src/RemoteEspNow/RemoteEspNow.cpp`**

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <string.h>
#include "RemoteEspNow.h"

RemoteEspNow remoteEspNow;

static void onRecvTrampoline(const uint8_t *mac, const uint8_t *data, int len) {
    remoteEspNow.onReceive(mac, data, len);
}

void RemoteEspNow::setup() {
    WiFi.mode(WIFI_STA);
    // Same C3 Supermini stability workaround as the controller; close range.
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    esp_wifi_set_channel(REMOTE_LINK_CHANNEL, WIFI_SECOND_CHAN_NONE);

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW init failed");
        return;
    }
    esp_now_register_recv_cb(onRecvTrampoline);
    setBroadcastPeer();
}

void RemoteEspNow::setBroadcastPeer() {
    static const uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    setPeer(bcast);
}

void RemoteEspNow::setPeer(const uint8_t mac[6]) {
    if (esp_now_is_peer_exist(peerMac_)) {
        esp_now_del_peer(peerMac_);
    }
    memcpy(peerMac_, mac, 6);
    esp_now_peer_info_t peer{};
    memcpy(peer.peer_addr, peerMac_, 6);
    peer.channel = REMOTE_LINK_CHANNEL;
    peer.encrypt = false;
    esp_now_add_peer(&peer);
}

void RemoteEspNow::sendThrottle(const ThrottleToControllerPacket &pkt) {
    esp_now_send(peerMac_, reinterpret_cast<const uint8_t *>(&pkt), sizeof(pkt));
}

void RemoteEspNow::onReceive(const uint8_t *senderMac, const uint8_t *data, int len) {
    if (len != static_cast<int>(sizeof(ControllerToThrottlePacket))) return;
    memcpy(&state_, data, sizeof(state_));
    memcpy(lastSenderMac_, senderMac, 6);
    lastRxMs_ = millis();
    hasState_ = true;
}
```

- [ ] **Step 3: Rewrite `throttle/src/main.cpp` to wire everything together**

```cpp
#include <Arduino.h>
#include <AceButton.h>
#include <Buzzer.h>
#include "RemoteLinkProtocol.h"
#include "RemotePins.h"
#include "RemoteLogic/RemoteLogic.h"
#include "RemoteEspNow/RemoteEspNow.h"

using namespace ace_button;

Buzzer buzzer(BUZZER_PIN);
AceButton button(BUTTON_PIN);
RemoteButtonState buttonState;

static uint8_t lastBeepCounter = 0;

static const uint16_t THROTTLE_TX_INTERVAL_MS = 20; // ~50 Hz
static uint32_t lastTxMs = 0;

void handleButton(AceButton *, uint8_t eventType, uint8_t) {
    if (eventType == AceButton::kEventClicked) {
        recordButtonEvent(buttonState, RemoteButtonEvent::ShortClick);
    } else if (eventType == AceButton::kEventLongPressed) {
        recordButtonEvent(buttonState, RemoteButtonEvent::LongPress);
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_RED_PIN, OUTPUT);
    pinMode(LED_GREEN_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    analogReadResolution(12); // 0..4095 raw Hall

    buzzer.setup();

    ButtonConfig *cfg = button.getButtonConfig();
    cfg->setEventHandler(handleButton);
    cfg->setFeature(ButtonConfig::kFeatureClick);
    cfg->setFeature(ButtonConfig::kFeatureLongPress);
    cfg->setFeature(ButtonConfig::kFeatureSuppressAfterLongPress);

    remoteEspNow.setup();
    buzzer.beepSystemStart();
}

static void driveLeds() {
    bool paired = false; // wired up in Task 4 (pairing/persistence)
    bool pairingMode = false;
    bool linkLost = !remoteEspNow.hasState() || isLinkLost(remoteEspNow.lastRxMs(), millis());
    bool armed = remoteEspNow.hasState() && remoteEspNow.state().armed;
    LedOutput out = renderLeds(pickLedPattern(paired, pairingMode, linkLost, armed), millis());
    digitalWrite(LED_RED_PIN, out.red ? HIGH : LOW);
    digitalWrite(LED_GREEN_PIN, out.green ? HIGH : LOW);
}

static void playRequestedBeep() {
    if (!remoteEspNow.hasState()) return;
    ControllerToThrottlePacket s = remoteEspNow.state();
    if (!remoteLinkCounterAdvanced(s.beepCommandCounter, lastBeepCounter)) return;
    switch (s.beepCommand) {
        case RemoteBeep::SystemStart:     buzzer.beepSystemStart(); break;
        case RemoteBeep::CalibrationStep: buzzer.beepCalibrationStep(); break;
        case RemoteBeep::Armed:           buzzer.beepArmedAlert(); break;
        case RemoteBeep::Disarmed:        buzzer.beepDisarmed(); break;
        case RemoteBeep::Alert:           buzzer.beepArmingBlocked(); break;
        default: break;
    }
}

void loop() {
    button.check();
    buzzer.handle();

    uint32_t now = millis();
    if (now - lastTxMs >= THROTTLE_TX_INTERVAL_MS) {
        lastTxMs = now;
        ThrottleToControllerPacket pkt{};
        pkt.hallRaw = analogRead(HALL_PIN);
        pkt.buttonEventCounter = buttonState.counter;
        pkt.buttonEventType = buttonState.type;
        remoteEspNow.sendThrottle(pkt);
    }

    playRequestedBeep();
    driveLeds();
}
```

- [ ] **Step 4: Verify the remote firmware builds**

```bash
cd /Users/rodrigo/dev/fly-controller/throttle && ~/.platformio/penv/bin/pio run -e remote_throttle
```
Expected: `SUCCESS`.

- [ ] **Step 5: Commit**

```bash
git add throttle/src/RemoteEspNow/ throttle/src/main.cpp
git commit -m "feat(throttle): ESP-NOW transport + full loop integration

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 4: Pairing broadcast + controller-MAC persistence (remote side)

**Files:**
- Create: `throttle/src/RemotePairing/RemotePairing.h`
- Create: `throttle/src/RemotePairing/RemotePairing.cpp`
- Modify: `throttle/src/main.cpp`

The remote persists the controller MAC in NVS. Unpaired (no saved MAC), it sends to broadcast. Holding the button for `PAIRING_HOLD_MS` enters pairing mode (LEDs alternate); while pairing, the first controller packet received teaches the remote the controller MAC, which is saved and switched to as the unicast peer.

- [ ] **Step 1: Write `throttle/src/RemotePairing/RemotePairing.h`**

```cpp
#ifndef REMOTE_PAIRING_H
#define REMOTE_PAIRING_H

#include <stdint.h>

class RemotePairing {
  public:
    void load();                       // read saved controller MAC from NVS
    bool isPaired() const { return paired_; }
    const uint8_t *peerMac() const { return mac_; }
    void save(const uint8_t mac[6]);   // persist + mark paired
    void clear();                      // forget pairing

  private:
    uint8_t mac_[6] = {0};
    bool paired_ = false;
};

extern RemotePairing remotePairing;

#endif // REMOTE_PAIRING_H
```

- [ ] **Step 2: Write `throttle/src/RemotePairing/RemotePairing.cpp`**

```cpp
#include <Preferences.h>
#include <string.h>
#include "RemotePairing.h"

RemotePairing remotePairing;

static const char *NVS_NAMESPACE = "remote";
static const char *NVS_KEY_MAC = "ctrlMac";

void RemotePairing::load() {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true); // read-only
    size_t len = prefs.getBytes(NVS_KEY_MAC, mac_, sizeof(mac_));
    prefs.end();
    paired_ = (len == sizeof(mac_));
}

void RemotePairing::save(const uint8_t mac[6]) {
    memcpy(mac_, mac, sizeof(mac_));
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false); // read-write
    prefs.putBytes(NVS_KEY_MAC, mac_, sizeof(mac_));
    prefs.end();
    paired_ = true;
}

void RemotePairing::clear() {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.remove(NVS_KEY_MAC);
    prefs.end();
    paired_ = false;
    memset(mac_, 0, sizeof(mac_));
}
```

- [ ] **Step 3: Wire pairing into `main.cpp`**

Add the include near the others:

```cpp
#include "RemotePairing/RemotePairing.h"
```

Add a pairing-mode state and a long-hold detector. Replace the `handleButton` and `driveLeds` functions and extend `setup()`/`loop()`:

In `setup()`, after `remoteEspNow.setup();` add:

```cpp
    remotePairing.load();
    if (remotePairing.isPaired()) {
        remoteEspNow.setPeer(remotePairing.peerMac());
    } else {
        remoteEspNow.setBroadcastPeer();
    }
```

Add a module-level pairing flag and update `driveLeds()` to use real values:

```cpp
static bool pairingMode = false;

static void driveLeds() {
    bool paired = remotePairing.isPaired();
    bool linkLost = !remoteEspNow.hasState() || isLinkLost(remoteEspNow.lastRxMs(), millis());
    bool armed = remoteEspNow.hasState() && remoteEspNow.state().armed;
    LedOutput out = renderLeds(pickLedPattern(paired, pairingMode, linkLost, armed), millis());
    digitalWrite(LED_RED_PIN, out.red ? HIGH : LOW);
    digitalWrite(LED_GREEN_PIN, out.green ? HIGH : LOW);
}
```

Add pairing capture at the end of `loop()` (before `driveLeds();`):

```cpp
    // While pairing, the first controller packet teaches us its MAC.
    if (pairingMode && remoteEspNow.hasState()) {
        remotePairing.save(remoteEspNow.lastSenderMac());
        remoteEspNow.setPeer(remotePairing.peerMac());
        pairingMode = false;
        buzzer.beepCalibrationComplete(); // audible pairing confirmation
    }
```

Enter pairing mode from a long press while unpaired — extend `handleButton`:

```cpp
void handleButton(AceButton *, uint8_t eventType, uint8_t) {
    if (eventType == AceButton::kEventClicked) {
        recordButtonEvent(buttonState, RemoteButtonEvent::ShortClick);
    } else if (eventType == AceButton::kEventLongPressed) {
        if (!remotePairing.isPaired()) {
            pairingMode = true;
            remoteEspNow.setBroadcastPeer();
        }
        recordButtonEvent(buttonState, RemoteButtonEvent::LongPress);
    }
}
```

> Note: while paired, a long press still emits `LongPress` (calibration) to the controller. Pairing
> mode is only entered when unpaired, matching the spec's "hold to pair" gesture on a fresh remote.

- [ ] **Step 4: Verify the remote firmware builds**

```bash
cd /Users/rodrigo/dev/fly-controller/throttle && ~/.platformio/penv/bin/pio run -e remote_throttle
```
Expected: `SUCCESS`.

- [ ] **Step 5: Commit**

```bash
git add throttle/src/RemotePairing/ throttle/src/main.cpp
git commit -m "feat(throttle): pairing broadcast + controller-MAC NVS persistence

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Self-Review

**Spec coverage (remote-firmware slice):**
- Remote reads Hall (analogRead) + sends raw → Task 3 (`pkt.hallRaw = analogRead`). ✓
- AceButton short/long events with counter dedup → Task 2 (`recordButtonEvent`) + Task 3 (`handleButton`). ✓
- Two LEDs with 5-state machine → Task 2 (`pickLedPattern`/`renderLeds`) + Task 3 (`driveLeds`). ✓
- Buzzer driven by received beep commands → Task 3 (`playRequestedBeep`). ✓
- Remote-side link-loss failsafe (blink + alert) → Task 2 (`isLinkLost`) + Task 3 (LinkLostBlink); local alert beep is the controller-driven `Alert` beep, and link-lost is shown on LEDs. ✓
- Pairing broadcast + persistence → Task 4. ✓
- TX power 8.5 dBm, fixed channel → Task 3 (`RemoteEspNow::setup`). ✓
- No WiFi portal on remote → only `WIFI_STA` for ESP-NOW. ✓

**Placeholder scan:** Task 3's `driveLeds()` intentionally hardcodes `paired=false`/`pairingMode=false` and is fully replaced in Task 4 — not a leftover placeholder but a staged build. All other steps contain concrete code.

**Type consistency:** `ThrottleToControllerPacket`/`ControllerToThrottlePacket`/`RemoteButtonEvent`/`RemoteBeep`/`remoteLinkCounterAdvanced` from the shared header; `RemoteLedPattern`/`LedOutput`/`RemoteButtonState`/`pickLedPattern`/`renderLeds`/`isLinkLost`/`recordButtonEvent` from `RemoteLogic.h`; `RemoteEspNow`/`RemotePairing` singletons used consistently across `main.cpp`.

**Deferred to Plan 3 / bench:** end-to-end pairing (needs controller side), real link behavior, and the controller's own pairing-mode UI. ESP-NOW tasks are build-verified here; functional verification is on hardware.

## Follow-on
- **Plan 3 — Controller integration**: `RemoteLink/` component (mirror transport), `Settings` (source + paired MAC), wireless `ReadFn` into `Throttle`, button-event → `handleButtonEvent()`, hybrid failsafe (500 ms ramp / 3 s disarm), beep forwarding, web portal source selector + pairing control.
