#pragma once

// Dashboard served as PROGMEM chunks streamed via AsyncResponseStream.
// Tokens (APP_VERSION, BUILD_DATE, BUILD_TIME, CONTROLLER_LABEL) are
// printed inline between static chunks — no heap String concatenation.
//
// Split points:
//   P1 → up to APP_VERSION insertion
//   P2 → between APP_VERSION and BUILD_DATE
//   P3 → space between BUILD_DATE and BUILD_TIME
//   P4 → between BUILD_TIME and CONTROLLER_LABEL
//   P5 → from after CONTROLLER_LABEL to end of </html>

static const char DASHBOARD_HTML_P1[] PROGMEM = R"rawliteral(<!DOCTYPE html><html><head><title>FlyController Painel</title><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><link rel="stylesheet" href="/config.css"><script src="/config-common.js" defer></script><script src="/dashboard.js" defer></script></head><body><div class="page"><div class="topbar"><a class="nav-btn active" href="/">Painel</a><a class="nav-btn" href="/telemetry">Telemetria</a><a class="nav-btn" href="/firmware">Firmware</a><a class="nav-btn" href="/logs-page">Registros</a><a class="nav-btn" href="/config">Configura&#xE7;&#xF5;es</a></div>
<div class="panel">
    <h1>FlyController Painel</h1>
    <div class="sub">Acesso r&#xE1;pido e status b&#xE1;sico do dispositivo</div>
</div>

<div class="grid">
    <div class="card">
        <div class="label">Vers&#xE3;o do Firmware</div>
        <div class="value">)rawliteral";

static const char DASHBOARD_HTML_P2[] PROGMEM = R"rawliteral(</div>
        <div class="sub">Build: )rawliteral";

static const char DASHBOARD_HTML_P3[] PROGMEM = " ";

static const char DASHBOARD_HTML_P4[] PROGMEM = R"rawliteral(</div>
    </div>
    <div class="card">
        <div class="label">Tipo de Controlador</div>
        <div class="value">)rawliteral";

static const char DASHBOARD_HTML_P5[] PROGMEM = R"rawliteral(</div>
        <div class="sub">Tempo ligado: <span id="uptime">--</span></div>
    </div>
    <div class="card">
        <div class="label">Tens&#xE3;o da Bateria</div>
        <div class="value" id="batteryVoltage">--</div>
        <div class="sub">Telemetria: <span id="freshness">--</span></div>
    </div>
    <div class="card">
        <div class="label">Status do Sistema</div>
        <div class="value" id="armed">--</div>
        <div class="sub" id="telemetryState">--</div>
    </div>
    <div class="card">
        <div class="label">Hor&#xED;metro</div>
        <div class="value" id="hourMeter">--</div>
        <div class="sub">Motor acumulado</div>
    </div>
</div>
</div></body></html>)rawliteral";

static const char DASHBOARD_JS[] PROGMEM = R"rawliteral(
const formatVoltage = (mv) => `${(mv / 1000).toFixed(2)} V`;
const fmtSeconds = s => {
    const h = Math.floor(s / 3600);
    const m = Math.floor((s % 3600) / 60);
    const sec = s % 60;
    return `${h}:${String(m).padStart(2,'0')}:${String(sec).padStart(2,'0')}`;
};

function loadDashboard() {
    fetchJson('/api/telemetry')
        .then((d) => {
            setText('batteryVoltage', formatVoltage(d.batteryVoltageMv || 0));
            setText('armed', d.armed ? 'ARMADO' : 'DESARMADO');
            setText('uptime', `${Math.floor((d.uptimeMs || 0) / 1000)} s`);
            setText('hourMeter', fmtSeconds(d.hourMeterSec || 0));
            if (!d.hasTelemetry) {
                setText('telemetryState', 'Sem dados de telemetria');
                setText('freshness', '--');
                return;
            }
            const age = Math.max(0, (d.uptimeMs || 0) - (d.lastTelemetryUpdateMs || 0));
            setText('telemetryState', age > 3000 ? 'DESATUALIZADO' : 'AO VIVO');
            setText('freshness', `${age} ms`);
        })
        .catch(() => {
            setText('telemetryState', 'Indispon\xEDvel');
        });
}

loadDashboard();
setInterval(loadDashboard, 1000);
)rawliteral";
