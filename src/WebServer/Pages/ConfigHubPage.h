#pragma once

#include "CommonLayout.h"

inline String renderConfigHubPage() {
    const char* body = R"rawliteral(
<div class="panel">
    <h1>Configurações</h1>
    <div class="sub">Escolha uma seção para configurar o controlador.</div>
</div>

<div class="grid">
    <a class="card link-card" href="/config/power">
        <div class="label">Bateria e Energia</div>
        <div class="value">Energia</div>
        <div class="sub">Capacidade da bateria, limites de tensão, controle de energia e resposta do acelerador.</div>
    </a>
    <a class="card link-card" href="/config/thermal">
        <div class="label">Proteção Térmica</div>
        <div class="value">Térmica</div>
        <div class="sub">Limites de proteção do motor e do ESC e pontos de redução de energia.</div>
    </a>
    <a class="card link-card" href="/config/bms">
        <div class="label">BMS Bluetooth</div>
        <div class="value">BMS</div>
        <div class="sub">Tipo de BMS, endereço Bluetooth e ferramentas de dispositivo BLE.</div>
    </a>
</div>
)rawliteral";

    PageSpec spec = {
        "FlyController - Configurações",
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
