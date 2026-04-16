#pragma once

#include <Arduino.h>

static const char CONFIG_POWER_PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>FlyController - Battery & Power</title>
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
            <a class="nav-btn active" href="/config/power">Power</a>
            <a class="nav-btn" href="/config/thermal">Thermal</a>
            <a class="nav-btn" href="/config/bms">BMS</a>
            <a class="nav-btn" href="/config/system">System</a>
        </div>

        <div class="panel">
            <h1>Battery & Power</h1>

            <form id="powerConfigForm">
                <h2>Battery Settings</h2>

                <div class="form-group">
                    <label for="batteryCapacity">Battery Capacity (Ah):</label>
                    <select id="batteryCapacity" name="batteryCapacity">
                        <option value="18">18 Ah</option>
                        <option value="34">34 Ah</option>
                        <option value="65">65 Ah</option>
                        <option value="custom">Custom (enter value)</option>
                    </select>
                    <input type="number" id="batteryCapacityCustom" name="batteryCapacityCustom" min="1" max="200" step="0.1" placeholder="Enter capacity in Ah" style="display: none; margin-top: 10px;">
                    <div class="info-text">Select from preset values or choose custom to enter your own capacity.</div>
                </div>

                <div class="form-group">
                    <label for="minVoltagePerCell">Minimum Voltage per Cell (V):</label>
                    <input type="number" id="minVoltagePerCell" name="minVoltagePerCell" min="2.5" max="4.5" step="0.01" required>
                    <div class="info-text">Minimum safe voltage per cell (typically 3.0V - 3.15V for LiPo).</div>
                    <div class="total-voltage">Total: <span id="minVoltageTotalValue">0.00</span> V (14 cells)</div>
                </div>

                <div class="form-group">
                    <label for="maxVoltagePerCell">Maximum Voltage per Cell (V):</label>
                    <input type="number" id="maxVoltagePerCell" name="maxVoltagePerCell" min="2.5" max="4.5" step="0.01" required>
                    <div class="info-text">Maximum safe voltage per cell (typically 4.1V - 4.2V for LiPo).</div>
                    <div class="total-voltage">Total: <span id="maxVoltageTotalValue">0.00</span> V (14 cells)</div>
                </div>

                <h2>Power Control Settings</h2>

                <div class="form-group">
                    <label for="powerControlEnabled" style="display: flex; align-items: center; gap: 10px; cursor: pointer;">
                        <input type="checkbox" id="powerControlEnabled" name="powerControlEnabled" style="width: auto;">
                        <span>Enable Power Control</span>
                    </label>
                    <div class="info-text">When enabled, power output is limited based on battery voltage, motor temperature, and ESC temperature. When disabled, full power is available without limitations.</div>
                </div>

                <h2>Throttle Response</h2>

                <div class="form-group">
                    <label for="throttleCurveGamma">Throttle curve (gamma):</label>
                    <input type="number" id="throttleCurveGamma" name="throttleCurveGamma" min="1" max="3" step="0.1" required>
                    <div class="info-text">Power-law curve: 1.0 = linear; higher values = less sensitive at low throttle, more responsive at high throttle.</div>
                </div>

                <div class="form-group">
                    <label for="configPin">PIN</label>
                    <input type="password" id="configPin" maxlength="8" placeholder="Required to save">
                </div>

                <button type="submit" id="saveButton">Save Power Settings</button>
                <div class="message" id="message"></div>
            </form>
        </div>
    </div>

    <script defer src="/config-power.js"></script>
</body>
</html>
)rawliteral";

static const char CONFIG_POWER_PAGE_JS[] PROGMEM = R"rawliteral(
const $ = (id) => document.getElementById(id);
const CELL_COUNT = 14;
const getPin = () => sessionStorage.getItem('cfgPin') || '';
const setPin = (v) => sessionStorage.setItem('cfgPin', v);

const showMessage = (text, kind) => {
    const el = $('message');
    if (!el) return;
    el.textContent = text;
    el.className = `message ${kind}`;
    el.style.display = 'block';
};

const updateVoltageTotals = () => {
    const minVoltagePerCell = parseFloat($('minVoltagePerCell').value) || 0;
    const maxVoltagePerCell = parseFloat($('maxVoltagePerCell').value) || 0;
    $('minVoltageTotalValue').textContent = (minVoltagePerCell * CELL_COUNT).toFixed(2);
    $('maxVoltageTotalValue').textContent = (maxVoltagePerCell * CELL_COUNT).toFixed(2);
};

const loadCurrentValues = () => {
    fetch('/api/config/power')
        .then((response) => response.json())
        .then((data) => {
            const capacityAh = data.batteryCapacity / 1000;
            if (capacityAh === 18 || capacityAh === 35 || capacityAh === 65) {
                $('batteryCapacity').value = capacityAh;
                $('batteryCapacityCustom').style.display = 'none';
            } else {
                $('batteryCapacity').value = 'custom';
                $('batteryCapacityCustom').value = capacityAh;
                $('batteryCapacityCustom').style.display = 'block';
            }

            $('minVoltagePerCell').value = ((data.batteryMinVoltage / 1000) / CELL_COUNT).toFixed(2);
            $('maxVoltagePerCell').value = ((data.batteryMaxVoltage / 1000) / CELL_COUNT).toFixed(2);
            $('powerControlEnabled').checked = data.powerControlEnabled || false;
            $('throttleCurveGamma').value = typeof data.throttleCurveGamma === 'number' ? data.throttleCurveGamma : '1.0';
            updateVoltageTotals();
        })
        .catch((error) => {
            console.error('Error loading power settings:', error);
            showMessage('Error loading current configuration', 'err');
        });
};

$('batteryCapacity').addEventListener('change', function() {
    if (this.value === 'custom') {
        $('batteryCapacityCustom').style.display = 'block';
        $('batteryCapacityCustom').focus();
    } else {
        $('batteryCapacityCustom').style.display = 'none';
    }
});

$('minVoltagePerCell').addEventListener('input', updateVoltageTotals);
$('maxVoltagePerCell').addEventListener('input', updateVoltageTotals);

$('powerConfigForm').addEventListener('submit', function(e) {
    e.preventDefault();

    const saveButton = $('saveButton');
    saveButton.disabled = true;
    $('message').style.display = 'none';

    const capacityAh = $('batteryCapacity').value === 'custom'
        ? parseFloat($('batteryCapacityCustom').value)
        : parseFloat($('batteryCapacity').value);

    if (!capacityAh || capacityAh < 1 || capacityAh > 200) {
        showMessage('Battery capacity must be between 1 and 200 Ah', 'err');
        saveButton.disabled = false;
        return;
    }

    const minVoltagePerCell = parseFloat($('minVoltagePerCell').value);
    const maxVoltagePerCell = parseFloat($('maxVoltagePerCell').value);
    const throttleCurveGamma = parseFloat($('throttleCurveGamma').value);

    if (isNaN(throttleCurveGamma) || throttleCurveGamma < 1 || throttleCurveGamma > 3) {
        showMessage('Throttle curve gamma must be between 1.0 and 3.0', 'err');
        saveButton.disabled = false;
        return;
    }

    const data = {
        batteryCapacity: Math.round(capacityAh * 1000),
        batteryMinVoltage: Math.round(minVoltagePerCell * CELL_COUNT * 1000),
        batteryMaxVoltage: Math.round(maxVoltagePerCell * CELL_COUNT * 1000),
        powerControlEnabled: $('powerControlEnabled').checked,
        throttleCurveGamma: throttleCurveGamma
    };

    const pin = $('configPin').value;
    setPin(pin);

    fetch('/api/config/power', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json', 'X-Config-Pin': pin },
        body: JSON.stringify(data)
    })
        .then((response) => response.text().then((text) => ({ ok: response.ok, text })))
        .then(({ ok, text }) => {
            showMessage(ok ? 'Power settings saved successfully!' : 'Error saving configuration: ' + text, ok ? 'ok' : 'err');
            saveButton.disabled = false;
        })
        .catch((error) => {
            showMessage('Error saving configuration: ' + error, 'err');
            saveButton.disabled = false;
        });
});

// Pre-fill PIN from sessionStorage
window.addEventListener('DOMContentLoaded', () => { $('configPin').value = getPin(); });

loadCurrentValues();
)rawliteral";
