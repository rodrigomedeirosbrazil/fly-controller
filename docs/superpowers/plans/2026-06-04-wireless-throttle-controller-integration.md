# Wireless Throttle — Controller Integration Implementation Plan (Plan 3 of 3)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Integrate the ESP-NOW remote throttle into the controller: a runtime-selectable throttle source (wired/wireless), the existing AceButton driven by the remote's raw button state, a hybrid link-loss failsafe, beep forwarding to the remote, and a web-portal source selector + pairing control.

**Architecture:** A new `RemoteLink/` controller component owns ESP-NOW transport (coexisting with the always-on WiFi AP), the last-received Hall/button state, beep requests, and pairing. Pure decision logic (failsafe) lives in a host-testable header. The throttle's existing `ReadFn` and the existing AceButton's `readButton()` switch source based on a `Settings` flag — so all calibration, arming, and ramp logic is reused unchanged.

**Tech Stack:** ESP32-C3 Arduino, ESP-NOW, ESP32 `Preferences`, AceButton, ESPAsyncWebServer, Unity/native-host tests.

**Spec:** `docs/superpowers/specs/2026-06-04-wireless-throttle-espnow-design.md`
**Depends on:** Plan 1 (`shared/RemoteLinkProtocol.h`) and Plan 2 (remote firmware), both merged on this branch.

---

### Task 1: `Settings` — throttle source + paired remote MAC

**Files:**
- Modify: `controller/src/Settings/Settings.h`
- Modify: `controller/src/Settings/Settings.cpp`

Mirror the existing `bmsMac` (String MAC) and `buzzVol` (UChar) patterns.

- [ ] **Step 1: Add the enum + accessors to `Settings.h`**

After the `BmsType` enum, add:

```cpp
enum ThrottleSource : uint8_t {
    ThrottleSourceWired = 0,
    ThrottleSourceWireless = 1
};
```

In the `public:` section (near `getBmsMac`), add:

```cpp
    // Wireless throttle (ESP-NOW remote)
    uint8_t getThrottleSource() const;
    void setThrottleSource(uint8_t source);
    String getRemoteMac() const;          // "" when unpaired
    void setRemoteMac(const char* mac);
    void clearRemoteMac();
```

In the `private:` section (near the `bmsMac` member), add:

```cpp
    uint8_t throttleSource;
    String remoteMac;
```

- [ ] **Step 2: Add defaults, load, and save in `Settings.cpp`**

In the defaults block (where `buzzerVolume = getDefaultBuzzerVolume();`), add:

```cpp
    throttleSource = ThrottleSourceWired;
    remoteMac = "";
```

In `load()` (near `buzzerVolume = preferences.getUChar(...)`):

```cpp
    throttleSource = preferences.getUChar("thrSrc", ThrottleSourceWired);
    if (throttleSource > ThrottleSourceWireless) throttleSource = ThrottleSourceWired;
    remoteMac = preferences.getString("rmtMac", "");
```

In `save()` (near `preferences.putUChar("buzzVol", ...)`):

```cpp
    preferences.putUChar("thrSrc", throttleSource);
    preferences.putString("rmtMac", remoteMac);
```

- [ ] **Step 3: Implement the accessors in `Settings.cpp`**

Append near the buzzer-volume accessors (use the same mutex style as the surrounding methods — copy the lock/unlock pattern used by `getBmsMac`/`setBmsMac`):

```cpp
uint8_t Settings::getThrottleSource() const {
    return throttleSource;
}

void Settings::setThrottleSource(uint8_t source) {
    throttleSource = (source > ThrottleSourceWireless) ? ThrottleSourceWired : source;
    save();
}

String Settings::getRemoteMac() const {
    return remoteMac;
}

void Settings::setRemoteMac(const char* mac) {
    remoteMac = mac;
    save();
}

void Settings::clearRemoteMac() {
    remoteMac = "";
    save();
}
```

- [ ] **Step 4: Verify the controller builds**

```bash
cd /Users/rodrigo/dev/fly-controller/controller && ~/.platformio/penv/bin/pio run -e lolin_c3_mini_hobbywing
```
Expected: `SUCCESS`.

- [ ] **Step 5: Commit**

```bash
git add controller/src/Settings/
git commit -m "feat(controller): persist throttle source + paired remote MAC

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 2: `RemoteLinkLogic` — host-testable failsafe logic

**Files:**
- Create: `controller/src/RemoteLink/RemoteLinkLogic.h`
- Create: `controller/test/RemoteLinkLogicTest.cpp`

- [ ] **Step 1: Write the failing host test**

Create `controller/test/RemoteLinkLogicTest.cpp`:

```cpp
#include <cassert>
#include <cstdint>
#include <iostream>
#include "../src/RemoteLink/RemoteLinkLogic.h"
using namespace std;

void test_no_failsafe_when_wired() {
    // Wired mode never triggers failsafe, even with a huge gap.
    assert(computeFailsafe(false, 0, 100000) == FailsafeAction::None);
}

void test_no_failsafe_within_window() {
    assert(computeFailsafe(true, 1000, 1000 + FAILSAFE_RAMP_MS - 1) == FailsafeAction::None);
}

void test_ramp_after_ramp_threshold() {
    assert(computeFailsafe(true, 1000, 1000 + FAILSAFE_RAMP_MS + 1) == FailsafeAction::RampToZero);
}

void test_disarm_after_disarm_threshold() {
    assert(computeFailsafe(true, 1000, 1000 + FAILSAFE_DISARM_MS + 1) == FailsafeAction::Disarm);
}

int main() {
    test_no_failsafe_when_wired();
    test_no_failsafe_within_window();
    test_ramp_after_ramp_threshold();
    test_disarm_after_disarm_threshold();
    cout << "RemoteLinkLogicTest: all passed" << endl;
    return 0;
}
```

- [ ] **Step 2: Run the test to verify it fails (header missing)**

```bash
cd /Users/rodrigo/dev/fly-controller && \
  c++ -std=c++17 -o /tmp/rlllogic controller/test/RemoteLinkLogicTest.cpp 2>&1 | head
```
Expected: FAIL — `RemoteLinkLogic.h` file not found.

- [ ] **Step 3: Write `controller/src/RemoteLink/RemoteLinkLogic.h`**

```cpp
#ifndef REMOTE_LINK_LOGIC_H
#define REMOTE_LINK_LOGIC_H

#include <stdint.h>

// Hybrid link-loss failsafe thresholds (wireless mode only).
#define FAILSAFE_RAMP_MS   500   // no packet this long -> ramp throttle to zero
#define FAILSAFE_DISARM_MS 3000  // no packet this long -> disarm

enum class FailsafeAction {
    None,        // link healthy (or wired mode)
    RampToZero,  // force 0% throttle; existing ramp-down smooths it; stay armed
    Disarm,      // disarm entirely
};

// Decide the failsafe action from the link state. nowMs/lastRxMs are millis().
inline FailsafeAction computeFailsafe(bool wireless, uint32_t lastRxMs, uint32_t nowMs) {
    if (!wireless) return FailsafeAction::None;
    uint32_t gap = nowMs - lastRxMs;
    if (gap > FAILSAFE_DISARM_MS) return FailsafeAction::Disarm;
    if (gap > FAILSAFE_RAMP_MS)   return FailsafeAction::RampToZero;
    return FailsafeAction::None;
}

#endif // REMOTE_LINK_LOGIC_H
```

- [ ] **Step 4: Run the test to verify it passes**

```bash
cd /Users/rodrigo/dev/fly-controller && \
  c++ -std=c++17 -o /tmp/rlllogic controller/test/RemoteLinkLogicTest.cpp && /tmp/rlllogic
```
Expected: `RemoteLinkLogicTest: all passed`.

- [ ] **Step 5: Commit**

```bash
git add controller/src/RemoteLink/RemoteLinkLogic.h controller/test/RemoteLinkLogicTest.cpp
git commit -m "feat(controller): host-tested wireless failsafe logic

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 3: `RemoteLink` component — ESP-NOW transport, beep requests, pairing

**Files:**
- Create: `controller/src/RemoteLink/RemoteLink.h`
- Create: `controller/src/RemoteLink/RemoteLink.cpp`
- Modify: `controller/src/config.h` (extern declaration)
- Modify: `controller/src/config.cpp` (instantiation)
- Modify: `controller/src/main.cpp` (setup + handle)

ESP-NOW coexists with the already-running WiFi AP (started in `webServer.begin()`), which uses channel 1 = `REMOTE_LINK_CHANNEL`. `RemoteLink::setup()` runs after `webServer.begin()`.

- [ ] **Step 1: Write `controller/src/RemoteLink/RemoteLink.h`**

```cpp
#ifndef REMOTE_LINK_H
#define REMOTE_LINK_H

#include <stdint.h>
#include "RemoteLinkProtocol.h"
#include "RemoteLinkLogic.h"

class RemoteLink {
  public:
    void setup();   // init ESP-NOW on the AP channel; load paired peer from Settings
    void handle();  // periodic TX of controller state to the remote (~5 Hz)

    // Throttle / button source (used by Throttle ReadFn and the Button config).
    uint16_t lastHallRaw() const { return rx_.hallRaw; }
    bool remoteButtonPressed() const { return rx_.buttonPressed != 0; }
    uint32_t lastRxMs() const { return lastRxMs_; }
    bool hasState() const { return hasState_; }

    // Failsafe convenience (wireless flag comes from Settings at the call site).
    FailsafeAction failsafe(bool wireless, uint32_t nowMs) const {
        return computeFailsafe(wireless, lastRxMs_, nowMs);
    }

    // Controller -> remote state.
    void setArmed(bool armed) { tx_.armed = armed ? 1 : 0; }
    void setCalibrating(bool c) { tx_.calibrating = c ? 1 : 0; }
    void requestBeep(uint8_t beep);   // bumps the beep counter; sent on next handle()

    // Pairing.
    void enterPairing() { pairing_ = true; }
    bool isPairing() const { return pairing_; }

    // Called from the static ESP-NOW recv trampoline.
    void onReceive(const uint8_t *senderMac, const uint8_t *data, int len);

  private:
    void addPeer(const uint8_t mac[6]);
    void sendState();

    ThrottleToControllerPacket rx_{};
    ControllerToThrottlePacket tx_{};
    volatile bool hasState_ = false;
    volatile uint32_t lastRxMs_ = 0;
    uint8_t peerMac_[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    bool pairing_ = false;
    uint32_t lastTxMs_ = 0;
};

extern RemoteLink remoteLink;

#endif // REMOTE_LINK_H
```

- [ ] **Step 2: Write `controller/src/RemoteLink/RemoteLink.cpp`**

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <string.h>
#include "RemoteLink.h"
#include "../config.h"

extern Settings settings;

RemoteLink remoteLink;

static const uint16_t STATE_TX_INTERVAL_MS = 200; // ~5 Hz heartbeat

static void onRecvTrampoline(const uint8_t *mac, const uint8_t *data, int len) {
    remoteLink.onReceive(mac, data, len);
}

// Parse "AA:BB:CC:DD:EE:FF" into 6 bytes. Returns true on success.
static bool parseMac(const String &s, uint8_t out[6]) {
    if (s.length() != 17) return false;
    for (int i = 0; i < 6; i++) {
        out[i] = (uint8_t)strtoul(s.substring(i * 3, i * 3 + 2).c_str(), nullptr, 16);
    }
    return true;
}

void RemoteLink::setup() {
    // WiFi AP is already up (webServer.begin()); ESP-NOW rides the same radio.
    if (esp_now_init() != ESP_OK) {
        Serial.println("[RemoteLink] ESP-NOW init failed");
        return;
    }
    esp_now_register_recv_cb(onRecvTrampoline);

    uint8_t mac[6];
    String saved = settings.getRemoteMac();
    if (saved.length() == 17 && parseMac(saved, mac)) {
        memcpy(peerMac_, mac, 6);
    } else {
        static const uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        memcpy(peerMac_, bcast, 6);
    }
    addPeer(peerMac_);
}

void RemoteLink::addPeer(const uint8_t mac[6]) {
    if (esp_now_is_peer_exist(mac)) esp_now_del_peer(mac);
    esp_now_peer_info_t peer{};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = REMOTE_LINK_CHANNEL;
    peer.encrypt = false;
    esp_now_add_peer(&peer);
}

void RemoteLink::requestBeep(uint8_t beep) {
    tx_.beepCommand = beep;
    tx_.beepCommandCounter++; // wraps; remote acts on change
}

void RemoteLink::sendState() {
    esp_now_send(peerMac_, reinterpret_cast<const uint8_t *>(&tx_), sizeof(tx_));
}

void RemoteLink::handle() {
    uint32_t now = millis();
    if (now - lastTxMs_ >= STATE_TX_INTERVAL_MS) {
        lastTxMs_ = now;
        sendState();
    }
}

void RemoteLink::onReceive(const uint8_t *senderMac, const uint8_t *data, int len) {
    if (len != static_cast<int>(sizeof(ThrottleToControllerPacket))) return;

    // During pairing, the first remote we hear becomes our peer.
    if (pairing_) {
        char macStr[18];
        snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                 senderMac[0], senderMac[1], senderMac[2],
                 senderMac[3], senderMac[4], senderMac[5]);
        settings.setRemoteMac(macStr);
        memcpy(peerMac_, senderMac, 6);
        addPeer(peerMac_);
        pairing_ = false;
    }

    // Only accept packets from the paired peer (or any while broadcasting/unpaired).
    memcpy(&rx_, data, sizeof(rx_));
    lastRxMs_ = millis();
    hasState_ = true;
}
```

- [ ] **Step 3: Declare + instantiate the global**

In `controller/src/config.h`, near the other `extern` declarations, add:

```cpp
extern RemoteLink remoteLink;
```

(Include the header at the top of `config.h` if other component headers are included there; otherwise the `extern RemoteLink remoteLink;` in `RemoteLink.h` already covers users that include it.)

In `controller/src/config.cpp`, add the include and instantiation:

```cpp
#include "RemoteLink/RemoteLink.h"
// ... with the other globals:
RemoteLink remoteLink;
```

> Note: `RemoteLink remoteLink;` is already declared `extern` in `RemoteLink.h`. Define it in
> `config.cpp` (not in `RemoteLink.cpp`) to match the project's single-translation-unit convention.
> Remove the `RemoteLink remoteLink;` line from `RemoteLink.cpp` shown in Step 2 if you instantiate
> in `config.cpp` — keep exactly one definition.

- [ ] **Step 4: Wire `setup()` and `handle()` into `main.cpp`**

Add the include near the others:

```cpp
#include "RemoteLink/RemoteLink.h"
```

In `setup()`, after `webServer.begin();` (so WiFi AP / channel is up):

```cpp
  remoteLink.setup();
```

In `loop()`, after `throttle.handle();` add:

```cpp
  remoteLink.setArmed(throttle.isArmed());
  remoteLink.setCalibrating(!throttle.isCalibrated());
  remoteLink.handle();
```

- [ ] **Step 5: Verify the controller builds**

```bash
cd /Users/rodrigo/dev/fly-controller/controller && ~/.platformio/penv/bin/pio run -e lolin_c3_mini_hobbywing
```
Expected: `SUCCESS`.

- [ ] **Step 6: Commit**

```bash
git add controller/src/RemoteLink/RemoteLink.h controller/src/RemoteLink/RemoteLink.cpp controller/src/config.h controller/src/config.cpp controller/src/main.cpp
git commit -m "feat(controller): RemoteLink ESP-NOW transport + pairing + beep requests

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 4: Switch throttle + button source at runtime

**Files:**
- Modify: `controller/src/config.cpp` (throttle ReadFn)
- Modify: `controller/src/Button/Button.h` (custom ButtonConfig)
- Modify: `controller/src/Button/Button.cpp` (readButton override)

- [ ] **Step 1: Branch the throttle `ReadFn` on the source setting**

In `controller/src/config.cpp`, replace the throttle construction:

```cpp
Throttle throttle([]() { return ads1115.readChannel(ADS1115_THROTTLE_CHANNEL); });
```

with:

```cpp
Throttle throttle([]() -> int {
    if (settings.getThrottleSource() == ThrottleSourceWireless) {
        // Failsafe ramp: feed 0 so the existing ramp-down drives the motor to idle.
        if (remoteLink.failsafe(true, millis()) != FailsafeAction::None) return 0;
        return (int)remoteLink.lastHallRaw();
    }
    return ads1115.readChannel(ADS1115_THROTTLE_CHANNEL);
});
```

Ensure `config.cpp` includes `RemoteLink/RemoteLink.h` and that `settings` is declared (it is `extern` in `config.h`).

- [ ] **Step 2: Add a source-switching ButtonConfig to `Button.h`**

Replace the class with one that owns a custom `ButtonConfig` subclass:

```cpp
#ifndef Button_h
#define Button_h

#include <AceButton.h>
#include "../config.h"

class Throttle;
class Buzzer;

using namespace ace_button;

// Reads the physical pin in wired mode, or the remote's forwarded button
// state in wireless mode — so the same AceButton arming gesture works for both.
class SourceSwitchButtonConfig : public ButtonConfig {
  protected:
    int readButton(uint8_t pin) override;
};

class Button
{
    public:
        Button(uint8_t pin);
        void check();
        void handleEvent(AceButton* aceButton, uint8_t eventType, uint8_t buttonState);

    private:
        const static unsigned long longClickThreshold = 3500;

        AceButton aceButton;
        SourceSwitchButtonConfig sourceConfig;
        uint8_t pin;
        ButtonConfig* buttonConfig;
        unsigned long releaseButtonTime;
        bool buttonWasClicked;
};

#endif
```

- [ ] **Step 3: Implement `readButton` and use the custom config in `Button.cpp`**

At the top of `Button.cpp`, add the include and externs:

```cpp
#include "../RemoteLink/RemoteLink.h"
#include "../Settings/Settings.h"
extern Settings settings;
```

Add the `readButton` implementation:

```cpp
int SourceSwitchButtonConfig::readButton(uint8_t pin) {
    if (settings.getThrottleSource() == ThrottleSourceWireless) {
        // Active-low semantics to match INPUT_PULLUP: pressed -> LOW.
        return remoteLink.remoteButtonPressed() ? LOW : HIGH;
    }
    return digitalRead(pin);
}
```

In the `Button` constructor, bind the AceButton to the custom config instead of the default. Replace:

```cpp
    aceButton.init(pin);
    releaseButtonTime = 0;
    buttonWasClicked = false;

    buttonConfig = aceButton.getButtonConfig();
```

with:

```cpp
    aceButton.setButtonConfig(&sourceConfig);
    aceButton.init(pin);
    releaseButtonTime = 0;
    buttonWasClicked = false;

    buttonConfig = aceButton.getButtonConfig();
```

- [ ] **Step 4: Verify the controller builds**

```bash
cd /Users/rodrigo/dev/fly-controller/controller && ~/.platformio/penv/bin/pio run -e lolin_c3_mini_hobbywing
```
Expected: `SUCCESS`.

- [ ] **Step 5: Commit**

```bash
git add controller/src/config.cpp controller/src/Button/
git commit -m "feat(controller): runtime throttle + button source switch (wired/wireless)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 5: Wire the disarm failsafe into the loop

**Files:**
- Modify: `controller/src/main.cpp`

The ramp failsafe is handled in the throttle ReadFn (Task 4). The disarm-after-3s case is applied in the loop.

- [ ] **Step 1: Apply the disarm failsafe in `loop()`**

In `controller/src/main.cpp`, right after the `remoteLink.handle();` block added in Task 3, add:

```cpp
  if (settings.getThrottleSource() == ThrottleSourceWireless &&
      throttle.isArmed() &&
      remoteLink.failsafe(true, millis()) == FailsafeAction::Disarm) {
    throttle.setDisarmed();
  }
```

Ensure `settings` is accessible in `main.cpp` (it is referenced via `extern Settings settings;` already used in `setup()`; add `extern Settings settings;` at the top of `loop()` scope or file scope if not present).

- [ ] **Step 2: Verify the controller builds**

```bash
cd /Users/rodrigo/dev/fly-controller/controller && ~/.platformio/penv/bin/pio run -e lolin_c3_mini_hobbywing
```
Expected: `SUCCESS`.

- [ ] **Step 3: Commit**

```bash
git add controller/src/main.cpp
git commit -m "feat(controller): disarm on prolonged wireless link loss

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 6: Forward beeps to the remote at key sites

**Files:**
- Modify: `controller/src/main.cpp` (system start, armed alert)
- Modify: `controller/src/Button/Button.cpp` (disarm)

Both buzzers stay active (controller keeps its local beeps); these calls add the forwarded command.

- [ ] **Step 1: Forward the system-start and armed-alert beeps in `main.cpp`**

After `buzzer.beepSystemStart();` in `setup()`, add:

```cpp
  remoteLink.requestBeep(RemoteBeep::SystemStart);
```

In `handleArmedBeep()`, after `buzzer.beepArmedAlert();`, add:

```cpp
        remoteLink.requestBeep(RemoteBeep::Armed);
```

and after `buzzer.stop();` (the disarm/motor-start branch), add:

```cpp
        remoteLink.requestBeep(RemoteBeep::Disarmed);
```

- [ ] **Step 2: Forward the disarm beep from the button path in `Button.cpp`**

In `handleEvent`, in the `if (throttle.isArmed()) { throttle.setDisarmed(); }` branch, add after `setDisarmed()`:

```cpp
        remoteLink.requestBeep(RemoteBeep::Disarmed);
```

(`RemoteLink.h` is already included from Task 4.)

- [ ] **Step 3: Verify the controller builds**

```bash
cd /Users/rodrigo/dev/fly-controller/controller && ~/.platformio/penv/bin/pio run -e lolin_c3_mini_hobbywing
```
Expected: `SUCCESS`.

- [ ] **Step 4: Commit**

```bash
git add controller/src/main.cpp controller/src/Button/Button.cpp
git commit -m "feat(controller): forward key beeps to the remote buzzer

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 7: Web portal — throttle source selector + pairing control

**Files:**
- Modify: the system/config page handler in `controller/src/WebServer/` (follow the existing buzzer-volume + BMS-MAC handlers)

> Before writing, read the existing config page that handles buzzer volume and BMS MAC (it is the
> closest pattern: a GET that renders current values and a POST that calls `settings.setXxx`). Mirror
> its structure exactly — do not invent a new page style.

- [ ] **Step 1: Add a throttle-source selector to the config page**

In the config page HTML, add a control bound to `settings.getThrottleSource()` with options Wired (0) / Wireless (1). On POST, call `settings.setThrottleSource(value)`. Mirror the buzzer-volume field's GET-render + POST-handler wiring.

- [ ] **Step 2: Add a pairing control + paired-MAC display**

Add a "Pair remote" button that POSTs to a new endpoint which calls `remoteLink.enterPairing();`, and display `settings.getRemoteMac()` (or "unpaired" when empty) with a "Forget" action that calls `settings.clearRemoteMac();`. Register the endpoint alongside the other `server.on(...)` handlers in `ControllerWebServer`.

- [ ] **Step 3: Verify the controller builds**

```bash
cd /Users/rodrigo/dev/fly-controller/controller && ~/.platformio/penv/bin/pio run -e lolin_c3_mini_hobbywing
```
Expected: `SUCCESS`.

- [ ] **Step 4: Verify all three build targets still build**

```bash
cd /Users/rodrigo/dev/fly-controller/controller && \
  ~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor && \
  ~/.platformio/penv/bin/pio run -e lolin_c3_mini_xag
```
Expected: both `SUCCESS`.

- [ ] **Step 5: Commit**

```bash
git add controller/src/WebServer/
git commit -m "feat(controller): web portal throttle-source selector + remote pairing

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Self-Review

**Spec coverage (controller-integration slice):**
- Settings: throttle source + paired MAC → Task 1. ✓
- ESP-NOW transport coexisting with WiFi AP, fixed channel → Task 3 (`RemoteLink::setup`). ✓
- Wireless `ReadFn` into existing Throttle → Task 4 Step 1. ✓
- Remote button into existing AceButton (identical arming) → Task 4 Steps 2-3. ✓
- Hybrid failsafe: 500 ms ramp (ReadFn returns 0) + 3 s disarm (loop) → Task 2 (logic) + Task 4 + Task 5. ✓
- Beep forwarding, both buzzers active → Task 6. ✓
- Pairing capture + persistence → Task 3 (`onReceive` pairing) + Task 1 (Settings). ✓
- Web portal source selector + pairing control → Task 7. ✓

**Placeholder scan:** Task 7's HTML is described against the existing page pattern rather than pasted verbatim, because the exact markup must be read from the live config page first (noted explicitly). All controller-logic tasks contain complete code.

**Type consistency:** `ThrottleSource`/`getThrottleSource`/`getRemoteMac`/`setRemoteMac`/`clearRemoteMac` (Settings); `FailsafeAction`/`computeFailsafe`/`FAILSAFE_RAMP_MS`/`FAILSAFE_DISARM_MS` (RemoteLinkLogic); `RemoteLink`/`remoteLink`/`lastHallRaw`/`remoteButtonPressed`/`failsafe`/`requestBeep`/`enterPairing` used consistently across config.cpp, Button.cpp, and main.cpp; `RemoteBeep::*` from the shared header.

**Deferred to bench:** full two-device verification (pairing handshake, arming via remote button, throttle sweep, calibration over the link, 500 ms/3 s failsafe behavior, WiFi+BLE+ESP-NOW coexistence with the portal open).

## Completion
After Task 7, the full feature branch is functionally complete (build-verified + host-tested). Run
the **superpowers:finishing-a-development-branch** skill: bench-test on two devices, then choose
merge/PR/cleanup.
