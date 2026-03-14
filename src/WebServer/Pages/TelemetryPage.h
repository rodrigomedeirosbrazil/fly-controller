#pragma once

#include "CommonLayout.h"

inline String renderTelemetryPage() {
    const char* body = R"rawliteral(
<div class="panel">
    <h1>Live Telemetry</h1>
    <div>Data status: <span id="statusBadge" class="status nodata">NO DATA</span></div>
</div>

<div class="grid">
    <div class="card">
        <div class="label">Battery Voltage</div>
        <div class="value" id="batteryVoltage">--</div>
    </div>
    <div class="card">
        <div class="label">Battery SoC (CC)</div>
        <div class="value" id="socCc">--</div>
    </div>
    <div class="card">
        <div class="label">Battery SoC (Voltage)</div>
        <div class="value" id="socVoltage">--</div>
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
)rawliteral";

    const char* script = R"rawliteral(
const fmtC = (mc) => `${(mc / 1000).toFixed(1)} C`;
const fmtV = (mv) => `${(mv / 1000).toFixed(2)} V`;
const fmtA = (ma) => `${(ma / 1000).toFixed(1)} A`;
const fmtKw = (kwx10) => `${(kwx10 / 10).toFixed(1)} kW`;

const setStatus = (kind) => {
    const badge = $('statusBadge');
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
    setText('socCc', `${data.batteryPercentCc || 0} %`);
    setText('socVoltage', `${data.batteryPercentVoltage || 0} %`);
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

    PageSpec spec = {
        "FlyController - Telemetry",
        "/telemetry",
        body,
        nullptr,
        script,
        nullptr
    };

    return renderPage(spec);
}
