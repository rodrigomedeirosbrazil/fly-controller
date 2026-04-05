#pragma once

#include <Arduino.h>

static const char CONFIG_SYSTEM_PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>FlyController - System</title>
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
            <a class="nav-btn" href="/config/bms">BMS</a>
            <a class="nav-btn active" href="/config/system">System</a>
        </div>

        <div class="panel">
            <h1>System</h1>

            <form id="systemConfigForm">
                <div class="form-group">
                    <label for="wifiAutoDisableAfterCalibration" style="display: flex; align-items: center; gap: 10px; cursor: pointer;">
                        <input type="checkbox" id="wifiAutoDisableAfterCalibration" name="wifiAutoDisableAfterCalibration" style="width: auto;">
                        <span>Disable Wi-Fi after throttle calibration</span>
                    </label>
                    <div class="info-text">When enabled, access point and web server are stopped automatically after throttle calibration completes.</div>
                </div>

                <button type="submit" id="saveButton">Save System Settings</button>
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
            $('wifiAutoDisableAfterCalibration').checked = data.wifiAutoDisableAfterCalibration !== false;
        })
        .catch((error) => {
            console.error('Error loading system settings:', error);
            showMessage('Error loading current configuration', 'err');
        });
};

$('systemConfigForm').addEventListener('submit', function(e) {
    e.preventDefault();

    const saveButton = $('saveButton');
    saveButton.disabled = true;
    $('message').style.display = 'none';

    fetch('/config/save', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
            wifiAutoDisableAfterCalibration: $('wifiAutoDisableAfterCalibration').checked
        })
    })
        .then((response) => response.text().then((text) => ({ ok: response.ok, text })))
        .then(({ ok, text }) => {
            showMessage(ok ? 'System settings saved successfully!' : 'Error saving configuration: ' + text, ok ? 'ok' : 'err');
            saveButton.disabled = false;
        })
        .catch((error) => {
            showMessage('Error saving configuration: ' + error, 'err');
            saveButton.disabled = false;
        });
});

loadCurrentValues();
)rawliteral";
