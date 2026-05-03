#pragma once

#include <Arduino.h>

static const char CONFIG_THERMAL_PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>FlyController - Proteção Térmica</title>
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
            <a class="nav-btn active" href="/config/thermal">Térmica</a>
            <a class="nav-btn" href="/config/bms">BMS</a>
        </div>

        <div class="panel">
            <h1>Proteção Térmica</h1>

            <form id="thermalConfigForm">
                <h2>Configurações de Temperatura do Motor</h2>

                <div class="form-group" id="motorTempSourceGroup" style="display:none">
                    <label for="motorTempSource">Fonte do Sensor de Temperatura do Motor:</label>
                    <select id="motorTempSource" name="motorTempSource">
                        <option value="0">CAN (ESC)</option>
                        <option value="1">ADS1115 (NTC externo)</option>
                    </select>
                    <div class="info-text">Selecione se a temperatura do motor vem do CAN bus do ESC ou do sensor NTC externo via ADS1115.</div>
                </div>

                <div class="form-group">
                    <label for="motorMaxTemp">Temperatura Máxima do Motor (C):</label>
                    <input type="number" id="motorMaxTemp" name="motorMaxTemp" min="0" max="150" step="1" required>
                    <div class="info-text">O motor será completamente desligado nesta temperatura.</div>
                </div>

                <div class="form-group">
                    <label for="motorTempReductionStart">Início da Redução de Temperatura do Motor (C):</label>
                    <input type="number" id="motorTempReductionStart" name="motorTempReductionStart" min="0" max="150" step="1" required>
                    <div class="info-text">A redução de energia começa nesta temperatura e aumenta linearmente até a temperatura máxima.</div>
                </div>

                <h2>Configurações de Temperatura do ESC</h2>

                <div class="form-group">
                    <label for="escMaxTemp">Temperatura Máxima do ESC (C):</label>
                    <input type="number" id="escMaxTemp" name="escMaxTemp" min="0" max="150" step="1" required>
                    <div class="info-text">O ESC será completamente desligado nesta temperatura.</div>
                </div>

                <div class="form-group">
                    <label for="escTempReductionStart">Início da Redução de Temperatura do ESC (C):</label>
                    <input type="number" id="escTempReductionStart" name="escTempReductionStart" min="0" max="150" step="1" required>
                    <div class="info-text">A redução de energia começa nesta temperatura e aumenta linearmente até a temperatura máxima.</div>
                </div>

                <div class="form-group">
                    <label for="configPin">PIN</label>
                    <input type="password" id="configPin" maxlength="8" placeholder="Necessário para salvar">
                </div>

                <button type="submit" id="saveButton">Salvar Configurações Térmicas</button>
                <div class="message" id="message"></div>
            </form>
        </div>
    </div>

    <script defer src="/config-thermal.js"></script>
</body>
</html>
)rawliteral";

static const char CONFIG_THERMAL_PAGE_JS[] PROGMEM = R"rawliteral(
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
    fetch('/api/config/thermal')
        .then((response) => response.json())
        .then((data) => {
            $('motorMaxTemp').value = data.motorMaxTemp / 1000;
            $('motorTempReductionStart').value = data.motorTempReductionStart / 1000;
            $('escMaxTemp').value = data.escMaxTemp / 1000;
            $('escTempReductionStart').value = data.escTempReductionStart / 1000;
            if (data.motorTempSource !== undefined) {
                $('motorTempSource').value = data.motorTempSource;
                $('motorTempSourceGroup').style.display = '';
            }
        })
        .catch((error) => {
            console.error('Error loading thermal settings:', error);
            showMessage('Erro ao carregar a configuração atual', 'err');
        });
};

$('thermalConfigForm').addEventListener('submit', function(e) {
    e.preventDefault();

    const saveButton = $('saveButton');
    saveButton.disabled = true;
    $('message').style.display = 'none';

    const data = {
        motorMaxTemp: Math.round(parseFloat($('motorMaxTemp').value) * 1000),
        motorTempReductionStart: Math.round(parseFloat($('motorTempReductionStart').value) * 1000),
        escMaxTemp: Math.round(parseFloat($('escMaxTemp').value) * 1000),
        escTempReductionStart: Math.round(parseFloat($('escTempReductionStart').value) * 1000)
    };
    if ($('motorTempSourceGroup').style.display !== 'none') {
        data.motorTempSource = parseInt($('motorTempSource').value, 10);
    }

    const pin = $('configPin').value;
    setPin(pin);

    fetch('/api/config/thermal', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json', 'X-Config-Pin': pin },
        body: JSON.stringify(data)
    })
        .then((response) => response.text().then((text) => ({ ok: response.ok, text })))
        .then(({ ok, text }) => {
            showMessage(ok ? 'Configurações térmicas salvas com sucesso!' : 'Erro ao salvar a configuração: ' + text, ok ? 'ok' : 'err');
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
