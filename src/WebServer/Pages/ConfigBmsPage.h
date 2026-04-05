#pragma once

#include <Arduino.h>

static const char CONFIG_BMS_PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>FlyController - Bluetooth BMS</title>
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
            <a class="nav-btn" href="/config/thermal">Thermal</a>
            <a class="nav-btn active" href="/config/bms">BMS</a>
            <a class="nav-btn" href="/config/system">System</a>
        </div>

        <div class="panel">
            <h1>Bluetooth BMS</h1>

            <form id="bmsConfigForm">
                <div class="form-group">
                    <label for="bmsType">BMS type:</label>
                    <select id="bmsType" name="bmsType">
                        <option value="0">Disabled</option>
                        <option value="1">JBD</option>
                        <option value="2">Daly (D2 BLE)</option>
                    </select>
                    <div class="info-text">Select the Bluetooth BMS backend used by the controller.</div>
                </div>

                <div class="form-group">
                    <label for="bmsMac">BMS Bluetooth address (MAC):</label>
                    <input type="text" id="bmsMac" name="bmsMac" maxlength="17" placeholder="A5:C2:39:2B:FC:4E">
                    <div class="info-text">Format: XX:XX:XX:XX:XX:XX (6 hex bytes with colons).</div>
                </div>

                <button type="submit" id="saveButton">Save BMS Settings</button>
                <div class="message" id="message"></div>
            </form>
        </div>
    </div>

    <script defer src="/config-bms.js"></script>
</body>
</html>
)rawliteral";

static const char CONFIG_BMS_PAGE_JS[] PROGMEM = R"rawliteral(
const $ = (id) => document.getElementById(id);

const showMessage = (text, kind) => {
    const el = $('message');
    if (!el) return;
    el.textContent = text;
    el.className = `message ${kind}`;
    el.style.display = 'block';
};

const loadCurrentValues = () => {
    fetch('/config/values')
        .then((response) => response.json())
        .then((data) => {
            $('bmsType').value = String(typeof data.bmsType === 'number' ? data.bmsType : 0);
            $('bmsMac').value = data.bmsMac || '';
        })
        .catch((error) => {
            console.error('Error loading BMS settings:', error);
            showMessage('Error loading current configuration', 'err');
        });
};

$('bmsConfigForm').addEventListener('submit', function(e) {
    e.preventDefault();

    const saveButton = $('saveButton');
    saveButton.disabled = true;
    $('message').style.display = 'none';

    const bmsType = parseInt($('bmsType').value, 10) || 0;
    const bmsMac = $('bmsMac').value.trim();

    if (bmsType !== 0 && !/^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$/.test(bmsMac)) {
        showMessage('BMS MAC must be in format XX:XX:XX:XX:XX:XX', 'err');
        saveButton.disabled = false;
        return;
    }

    fetch('/config/save', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
            bmsType: bmsType,
            bmsMac: bmsMac
        })
    })
        .then((response) => response.text().then((text) => ({ ok: response.ok, text })))
        .then(({ ok, text }) => {
            showMessage(ok ? 'BMS settings saved successfully!' : 'Error saving configuration: ' + text, ok ? 'ok' : 'err');
            saveButton.disabled = false;
        })
        .catch((error) => {
            showMessage('Error saving configuration: ' + error, 'err');
            saveButton.disabled = false;
        });
});

loadCurrentValues();
)rawliteral";
