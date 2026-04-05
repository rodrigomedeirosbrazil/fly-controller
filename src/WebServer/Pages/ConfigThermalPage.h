#pragma once

#include <Arduino.h>

static const char CONFIG_THERMAL_PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>FlyController - Thermal Protection</title>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="stylesheet" href="/config.css">
</head>
<body>
    <div class="page">
        <div class="topbar">
            <a class="nav-btn" href="/">Dashboard</a>
            <a class="nav-btn" href="/telemetry">Telemetry</a>
            <a class="nav-btn" href="/firmware">Firmware</a>
            <a class="nav-btn" href="/logs-page">Logs</a>
            <a class="nav-btn active" href="/config">Configuration</a>
        </div>

        <div class="subnav">
            <a class="nav-btn" href="/config/power">Power</a>
            <a class="nav-btn active" href="/config/thermal">Thermal</a>
            <a class="nav-btn" href="/config/bms">BMS</a>
            <a class="nav-btn" href="/config/system">System</a>
        </div>

        <div class="panel">
            <h1>Thermal Protection</h1>

            <form id="thermalConfigForm">
                <h2>Motor Temperature Settings</h2>

                <div class="form-group">
                    <label for="motorMaxTemp">Maximum Motor Temperature (C):</label>
                    <input type="number" id="motorMaxTemp" name="motorMaxTemp" min="0" max="150" step="1" required>
                    <div class="info-text">Motor will be completely disabled at this temperature.</div>
                </div>

                <div class="form-group">
                    <label for="motorTempReductionStart">Motor Temperature Reduction Start (C):</label>
                    <input type="number" id="motorTempReductionStart" name="motorTempReductionStart" min="0" max="150" step="1" required>
                    <div class="info-text">Power reduction begins at this temperature and increases linearly until maximum temperature.</div>
                </div>

                <h2>ESC Temperature Settings</h2>

                <div class="form-group">
                    <label for="escMaxTemp">Maximum ESC Temperature (C):</label>
                    <input type="number" id="escMaxTemp" name="escMaxTemp" min="0" max="150" step="1" required>
                    <div class="info-text">ESC will be completely disabled at this temperature.</div>
                </div>

                <div class="form-group">
                    <label for="escTempReductionStart">ESC Temperature Reduction Start (C):</label>
                    <input type="number" id="escTempReductionStart" name="escTempReductionStart" min="0" max="150" step="1" required>
                    <div class="info-text">Power reduction begins at this temperature and increases linearly until maximum temperature.</div>
                </div>

                <button type="submit" id="saveButton">Save Thermal Settings</button>
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
        })
        .catch((error) => {
            console.error('Error loading thermal settings:', error);
            showMessage('Error loading current configuration', 'err');
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

    fetch('/api/config/thermal', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(data)
    })
        .then((response) => response.text().then((text) => ({ ok: response.ok, text })))
        .then(({ ok, text }) => {
            showMessage(ok ? 'Thermal settings saved successfully!' : 'Error saving configuration: ' + text, ok ? 'ok' : 'err');
            saveButton.disabled = false;
        })
        .catch((error) => {
            showMessage('Error saving configuration: ' + error, 'err');
            saveButton.disabled = false;
        });
});

loadCurrentValues();
)rawliteral";
