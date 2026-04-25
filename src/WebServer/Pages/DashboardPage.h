#pragma once

#include "CommonLayout.h"

inline String renderDashboardPage() {
    const char* body = R"rawliteral(
<div class="panel">
    <h1>FlyController Painel</h1>
    <div class="sub">Acesso rápido e status básico do dispositivo</div>
</div>

<div class="grid">
    <div class="card">
        <div class="label">Versão do Firmware</div>
        <div class="value">%APP_VERSION%</div>
        <div class="sub">Build: %BUILD_DATE% %BUILD_TIME%</div>
    </div>
    <div class="card">
        <div class="label">Tipo de Controlador</div>
        <div class="value">%CONTROLLER%</div>
        <div class="sub">Tempo ligado: <span id="uptime">--</span></div>
    </div>
    <div class="card">
        <div class="label">Tensão da Bateria</div>
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
)rawliteral";

    const char* script = R"rawliteral(
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
            setText('telemetryState', 'Indisponível');
        });
}

loadDashboard();
setInterval(loadDashboard, 1000);
)rawliteral";

    PageSpec spec = {
        "FlyController Painel",
        "/",
        body,
        nullptr,
        script,
        nullptr,
        nullptr,
        nullptr
    };

    return renderPage(spec);
}
