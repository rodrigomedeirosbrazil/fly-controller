#pragma once

#include "CommonLayout.h"

inline String renderConfigHubPage() {
    const char* body = R"rawliteral(
<div class="panel">
    <h1>Configuration</h1>
    <div class="sub">Choose a section to configure the controller.</div>
</div>

<div class="grid">
    <a class="card link-card" href="/config/power">
        <div class="label">Battery & Power</div>
        <div class="value">Power</div>
        <div class="sub">Battery capacity, voltage limits, power control, and throttle response.</div>
    </a>
    <a class="card link-card" href="/config/thermal">
        <div class="label">Thermal Protection</div>
        <div class="value">Thermal</div>
        <div class="sub">Motor and ESC protection thresholds and power reduction points.</div>
    </a>
    <a class="card link-card" href="/config/bms">
        <div class="label">Bluetooth BMS</div>
        <div class="value">BMS</div>
        <div class="sub">BMS type, Bluetooth address, and BLE device tools.</div>
    </a>
    <a class="card link-card" href="/config/system">
        <div class="label">Device Behavior</div>
        <div class="value">System</div>
        <div class="sub">Wi-Fi behavior and small system-level options.</div>
    </a>
</div>
)rawliteral";

    PageSpec spec = {
        "FlyController - Configuration",
        "/config",
        body,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    };

    return renderPage(spec);
}
