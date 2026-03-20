#pragma once

#include "CommonLayout.h"

inline String renderDashboardPage() {
    const char* body = R"rawliteral(
<div class="panel">
    <h1>FlyController Dashboard</h1>
    <div class="sub">Quick access and basic device status</div>
</div>

<div class="grid">
    <div class="card">
        <div class="label">Firmware Version</div>
        <div class="value">%APP_VERSION%</div>
        <div class="sub">Build: %BUILD_DATE% %BUILD_TIME%</div>
    </div>
    <div class="card">
        <div class="label">Controller Type</div>
        <div class="value">%CONTROLLER%</div>
        <div class="sub">Uptime: <span id="uptime">--</span></div>
    </div>
    <div class="card">
        <div class="label">Battery Voltage</div>
        <div class="value" id="batteryVoltage">--</div>
        <div class="sub">Telemetry freshness: <span id="freshness">--</span></div>
    </div>
    <div class="card">
        <div class="label">System Status</div>
        <div class="value" id="armed">--</div>
        <div class="sub" id="telemetryState">--</div>
    </div>
</div>
)rawliteral";

    const char* script = R"rawliteral(
const formatVoltage = (mv) => `${(mv / 1000).toFixed(2)} V`;

function loadDashboard() {
    fetchJson('/api/telemetry')
        .then((d) => {
            setText('batteryVoltage', formatVoltage(d.batteryVoltageMv || 0));
            setText('armed', d.armed ? 'ARMED' : 'DISARMED');
            setText('uptime', `${Math.floor((d.uptimeMs || 0) / 1000)} s`);
            if (!d.hasTelemetry) {
                setText('telemetryState', 'No telemetry data');
                setText('freshness', '--');
                return;
            }
            const age = Math.max(0, (d.uptimeMs || 0) - (d.lastTelemetryUpdateMs || 0));
            setText('telemetryState', age > 3000 ? 'STALE' : 'LIVE');
            setText('freshness', `${age} ms`);
        })
        .catch(() => {
            setText('telemetryState', 'Unavailable');
        });
}

loadDashboard();
setInterval(loadDashboard, 1000);
)rawliteral";

    PageSpec spec = {
        "FlyController Dashboard",
        "/",
        body,
        nullptr,
        script,
        nullptr,
        nullptr
    };

    return renderPage(spec);
}
