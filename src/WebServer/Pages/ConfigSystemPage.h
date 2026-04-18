#pragma once

#include <Arduino.h>

static const char CONFIG_SYSTEM_PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>FlyController - Sistema</title>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="stylesheet" href="/config.css">
</head>
<body>
    <div class="page">
        <div class="topbar">
            <a class="nav-btn" href="/">Painel</a>
            <a class="nav-btn" href="/telemetry">Telemetria</a>
            <a class="nav-btn" href="/firmware">Firmware</a>
            <a class="nav-btn" href="/logs-page">Registros</a>
            <a class="nav-btn active" href="/config">Configurações</a>
        </div>

        <div class="subnav">
            <a class="nav-btn" href="/config/power">Energia</a>
            <a class="nav-btn" href="/config/thermal">Térmica</a>
            <a class="nav-btn" href="/config/bms">BMS</a>
            <a class="nav-btn active" href="/config/system">Sistema</a>
        </div>

        <div class="panel">
            <h1>Sistema</h1>

            <form id="systemConfigForm">
                <div class="form-group">
                    <label for="wifiAutoDisableAfterCalibration" style="display: flex; align-items: center; gap: 10px; cursor: pointer;">
                        <input type="checkbox" id="wifiAutoDisableAfterCalibration" name="wifiAutoDisableAfterCalibration" style="width: auto;">
                        <span>Desativar Wi-Fi após calibração do acelerador</span>
                    </label>
                    <div class="info-text">Quando ativado, o ponto de acesso e o servidor web são encerrados automaticamente após a conclusão da calibração do acelerador.</div>
                </div>

                <div class="form-group">
                    <label for="configPin">PIN</label>
                    <input type="password" id="configPin" maxlength="8" placeholder="Necessário para salvar">
                </div>

                <button type="submit" id="saveButton">Salvar Configurações do Sistema</button>
                <div class="message" id="message"></div>
            </form>
        </div>
    </div>

    <script defer src="/config-system.js"></script>
</body>
</html>
)rawliteral";

static const char CONFIG_SYSTEM_PAGE_JS[] PROGMEM = R"rawliteral(
const $ = (id) => document.getElementById(id);
const getPin = () => sessionStorage.getItem('cfgPin') || '';
const setPin = (v) => sessionStorage.setItem('cfgPin', v);

const showMessage = (text, kind) => {
    const el = $('message');
    if (!el) return;
    el.textContent = text;
    el.className = `message ${kind}`;
    el.style.display = 'block';
};

const loadCurrentValues = () => {
    fetch('/api/config/system')
        .then((response) => response.json())
        .then((data) => {
            $('wifiAutoDisableAfterCalibration').checked = data.wifiAutoDisableAfterCalibration !== false;
        })
        .catch((error) => {
            console.error('Error loading system settings:', error);
            showMessage('Erro ao carregar a configuração atual', 'err');
        });
};

$('systemConfigForm').addEventListener('submit', function(e) {
    e.preventDefault();

    const saveButton = $('saveButton');
    saveButton.disabled = true;
    $('message').style.display = 'none';

    const pin = $('configPin').value;
    setPin(pin);

    fetch('/api/config/system', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json', 'X-Config-Pin': pin },
        body: JSON.stringify({
            wifiAutoDisableAfterCalibration: $('wifiAutoDisableAfterCalibration').checked
        })
    })
        .then((response) => response.text().then((text) => ({ ok: response.ok, text })))
        .then(({ ok, text }) => {
            showMessage(ok ? 'Configurações do sistema salvas com sucesso!' : 'Erro ao salvar a configuração: ' + text, ok ? 'ok' : 'err');
            saveButton.disabled = false;
        })
        .catch((error) => {
            showMessage('Erro ao salvar a configuração: ' + error, 'err');
            saveButton.disabled = false;
        });
});

window.addEventListener('DOMContentLoaded', () => { $('configPin').value = getPin(); });

loadCurrentValues();
)rawliteral";
