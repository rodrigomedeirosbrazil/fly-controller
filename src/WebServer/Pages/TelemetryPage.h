#pragma once

#include <Arduino.h>

static const char TELEMETRY_PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>FlyController - Telemetry</title>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
    <meta name="theme-color" content="#0b74de">
    <meta name="mobile-web-app-capable" content="yes">
    <meta name="apple-mobile-web-app-capable" content="yes">
    <link rel="stylesheet" href="/config.css">
</head>
<body class="telemetry-page">
    <div class="page page-telemetry">
        <div class="topbar">
            <a class="nav-btn" href="/">Dashboard</a>
            <a class="nav-btn active" href="/telemetry">Telemetry</a>
            <a class="nav-btn" href="/firmware">Firmware</a>
            <a class="nav-btn" href="/logs-page">Logs</a>
            <a class="nav-btn" href="/config">Configuration</a>
        </div>

        <div class="telemetry-shell">
            <div class="panel telemetry-header">
                <h1>Live Telemetry</h1>
                <div>Data status: <span id="statusBadge" class="status nodata">NO DATA</span></div>
            </div>

            <div class="panel wake-panel">
                <div class="wake-panel-row">
                    <div>
                        <div class="label">Screen</div>
                        <div class="wake-status-line">
                            <span id="wakeStatusBadge" class="status status-secondary">INACTIVE</span>
                            <span id="wakeSupportHint" class="sub">Trying automatic keep-awake...</span>
                        </div>
                    </div>
                    <button type="button" id="wakeToggleButton" class="btn btn-sm wake-button">Keep Screen Awake</button>
                </div>
                <div class="sub wake-help" id="wakeHelp">The page will try automatically first. If the screen still turns off, tap the button once.</div>
            </div>

            <div class="grid telemetry-grid">
                <div class="card">
                    <div class="label">Battery Voltage</div>
                    <div class="value" id="batteryVoltage">--</div>
                </div>
                <div class="card">
                    <div class="label">Battery SoC</div>
                    <div class="value" id="soc">--</div>
                    <div class="sub" id="socCc">--</div>
                </div>
                <div class="card">
                    <div class="label">Power</div>
                    <div class="value" id="powerKw">--</div>
                    <div class="sub" id="powerPercent">--</div>
                </div>
                <div class="card">
                    <div class="label">Throttle</div>
                    <div class="value" id="throttlePercent">--</div>
                    <div class="sub" id="throttleRaw">--</div>
                </div>
                <div class="card">
                    <div class="label">Motor</div>
                    <div class="value" id="motorTemp">--</div>
                    <div class="sub" id="rpm">--</div>
                </div>
                <div class="card">
                    <div class="label">ESC</div>
                    <div class="value" id="escTemp">--</div>
                    <div class="sub" id="escCurrent">--</div>
                </div>
                <div class="card">
                    <div class="label">System</div>
                    <div class="value" id="armed">--</div>
                    <div class="sub" id="freshness">--</div>
                </div>
                <div class="card" id="bmsCard" style="display: none;">
                    <div class="label">BMS</div>
                    <div class="value" id="bmsTempMax">--</div>
                    <div class="sub" id="bmsCells">--</div>
                </div>
            </div>
        </div>
    </div>

    <script defer src="/telemetry.js"></script>
</body>
</html>
)rawliteral";

static const char TELEMETRY_PAGE_JS[] PROGMEM = R"rawliteral(
const $ = (id) => document.getElementById(id);

const setText = (id, value) => {
    const el = $(id);
    if (!el) return;
    if (el.textContent !== value) {
        el.textContent = value;
    }
};

const fetchJson = (url) => fetch(url).then((response) => response.json());

const fmtC = (mc) => `${(mc / 1000).toFixed(1)} C`;
const fmtV = (mv) => `${(mv / 1000).toFixed(2)} V`;
const fmtA = (ma) => `${(ma / 1000).toFixed(1)} A`;
const fmtKw = (kwx10) => `${(kwx10 / 10).toFixed(1)} kW`;

const setStatus = (kind) => {
    const badge = $('statusBadge');
    if (!badge) return;
    badge.className = `status ${kind}`;
    badge.textContent = kind === 'live' ? 'LIVE' : (kind === 'stale' ? 'STALE' : 'NO DATA');
};

const renderTelemetry = (data) => {
    const av = data.availability || {};

    if (!data.hasTelemetry) {
        setStatus('nodata');
        setText('freshness', 'Waiting for telemetry');
    } else {
        const age = data.uptimeMs - data.lastTelemetryUpdateMs;
        setStatus(age > 3000 ? 'stale' : 'live');
        setText('freshness', `Last update ${Math.max(0, age)} ms ago`);
    }

    setText('batteryVoltage', fmtV(data.batteryVoltageMv || 0));
    setText('soc', `${data.batteryPercentVoltage || 0} %`);
    setText('socCc', `CC: ${data.batteryPercentCc ?? 0} %`);
    setText('powerKw', av.powerKw ? fmtKw(data.powerKwX10 ?? 0) : 'N/A');
    setText('powerPercent', `Limit: ${data.powerPercent || 0} %`);
    setText('throttlePercent', `${data.throttlePercent || 0} %`);
    setText('throttleRaw', `Raw: ${data.throttleRaw || 0}`);
    setText('motorTemp', fmtC(data.motorTempMc || 0));
    setText('rpm', av.rpm ? `${data.rpm ?? 0} rpm` : 'N/A');
    setText('escTemp', fmtC(data.escTempMc || 0));
    setText('escCurrent', av.current ? fmtA(data.escCurrentMa ?? 0) : 'N/A');
    setText('armed', data.armed ? 'ARMED' : 'DISARMED');

    const bmsCard = $('bmsCard');
    if (data.bms && data.bms.available) {
        bmsCard.style.display = '';
        setText('bmsTempMax', data.bms.tempMaxC != null ? `${data.bms.tempMaxC} C` : '--');
        if (data.bms.cellMinMv != null && data.bms.cellMaxMv != null) {
            setText('bmsCells', `Cell: ${data.bms.cellMinMv}-${data.bms.cellMaxMv} mV, delta ${data.bms.cellDeltaMv ?? '--'} mV`);
        } else {
            setText('bmsCells', '--');
        }
    } else {
        bmsCard.style.display = 'none';
    }
};

const loadTelemetry = () => {
    fetchJson('/api/telemetry')
        .then(renderTelemetry)
        .catch(() => setStatus('nodata'));
};

loadTelemetry();
setInterval(loadTelemetry, 1000);
)rawliteral";
