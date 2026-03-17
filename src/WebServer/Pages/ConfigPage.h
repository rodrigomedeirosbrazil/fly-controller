#pragma once

#include "CommonLayout.h"

inline String renderConfigPage() {
    const char* body = R"rawliteral(
<div class="panel">
    <h1>FlyController Configuration</h1>

    <form id="configForm">
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
            <div class="total-voltage" id="minVoltageTotal">Total: <span id="minVoltageTotalValue">0.00</span> V (14 cells)</div>
        </div>

        <div class="form-group">
            <label for="maxVoltagePerCell">Maximum Voltage per Cell (V):</label>
            <input type="number" id="maxVoltagePerCell" name="maxVoltagePerCell" min="2.5" max="4.5" step="0.01" required>
            <div class="info-text">Maximum safe voltage per cell (typically 4.1V - 4.2V for LiPo).</div>
            <div class="total-voltage" id="maxVoltageTotal">Total: <span id="maxVoltageTotalValue">0.00</span> V (14 cells)</div>
        </div>

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
            <div class="info-text">Power-law curve: 1.0 = linear; higher values = less sensitive at low throttle, more responsive at high throttle. Range: 1.0 to 3.0.</div>
        </div>

        <h2>JBD BMS</h2>

        <div class="form-group">
            <label for="jbdBmsEnabled" style="display: flex; align-items: center; gap: 10px; cursor: pointer;">
                <input type="checkbox" id="jbdBmsEnabled" name="jbdBmsEnabled" style="width: auto;">
                <span>Use JBD BMS</span>
            </label>
            <div class="info-text">When enabled, the controller will connect to a JBD BMS over Bluetooth for battery voltage, current, and cell data (fallback when ESC telemetry is unavailable).</div>
        </div>

        <div class="form-group">
            <label for="jbdBmsMac">BMS Bluetooth address (MAC):</label>
            <input type="text" id="jbdBmsMac" name="jbdBmsMac" maxlength="17" placeholder="A5:C2:39:2B:FC:4E">
            <div class="info-text">Get the MAC from a BLE scanner app. Format: XX:XX:XX:XX:XX:XX (6 hex bytes with colons).</div>
        </div>

        <h2>Wi-Fi Settings</h2>

        <div class="form-group">
            <label for="wifiAutoDisableAfterCalibration" style="display: flex; align-items: center; gap: 10px; cursor: pointer;">
                <input type="checkbox" id="wifiAutoDisableAfterCalibration" name="wifiAutoDisableAfterCalibration" style="width: auto;">
                <span>Disable Wi-Fi after throttle calibration</span>
            </label>
            <div class="info-text">When enabled, access point and web server are stopped automatically after throttle calibration completes.</div>
        </div>

        <button type="submit" id="saveButton">Save Configuration</button>
        <div class="message" id="message"></div>
    </form>
</div>
)rawliteral";

    const char* script = R"rawliteral(
const CELL_COUNT = 14;

const updateVoltageTotals = () => {
    const minVoltagePerCell = parseFloat($('minVoltagePerCell').value) || 0;
    const maxVoltagePerCell = parseFloat($('maxVoltagePerCell').value) || 0;
    const minVoltageTotal = minVoltagePerCell * CELL_COUNT;
    const maxVoltageTotal = maxVoltagePerCell * CELL_COUNT;
    setText('minVoltageTotalValue', minVoltageTotal.toFixed(2));
    setText('maxVoltageTotalValue', maxVoltageTotal.toFixed(2));
};

const loadCurrentValues = () => {
    fetchJson('/config/values')
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

            const minVoltagePerCell = (data.batteryMinVoltage / 1000) / CELL_COUNT;
            const maxVoltagePerCell = (data.batteryMaxVoltage / 1000) / CELL_COUNT;
            $('minVoltagePerCell').value = minVoltagePerCell.toFixed(2);
            $('maxVoltagePerCell').value = maxVoltagePerCell.toFixed(2);
            updateVoltageTotals();

            $('motorMaxTemp').value = data.motorMaxTemp / 1000;
            $('motorTempReductionStart').value = data.motorTempReductionStart / 1000;

            $('escMaxTemp').value = data.escMaxTemp / 1000;
            $('escTempReductionStart').value = data.escTempReductionStart / 1000;

            $('powerControlEnabled').checked = data.powerControlEnabled || false;
            if (typeof data.throttleCurveGamma === 'number') {
                $('throttleCurveGamma').value = data.throttleCurveGamma;
            } else {
                $('throttleCurveGamma').value = '1.0';
            }
            $('jbdBmsEnabled').checked = data.jbdBmsEnabled !== false;
            $('jbdBmsMac').value = data.jbdBmsMac || '';
            $('wifiAutoDisableAfterCalibration').checked = data.wifiAutoDisableAfterCalibration !== false;
        })
        .catch((error) => {
            console.error('Error loading values:', error);
            showMessage('message', 'Error loading current configuration', 'err');
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

$('configForm').addEventListener('submit', function(e) {
    e.preventDefault();

    const saveButton = $('saveButton');
    saveButton.disabled = true;
    $('message').style.display = 'none';

    let capacityAh;
    if ($('batteryCapacity').value === 'custom') {
        capacityAh = parseFloat($('batteryCapacityCustom').value);
    } else {
        capacityAh = parseFloat($('batteryCapacity').value);
    }

    if (!capacityAh || capacityAh < 1 || capacityAh > 200) {
        showMessage('message', 'Battery capacity must be between 1 and 200 Ah', 'err');
        saveButton.disabled = false;
        return;
    }

    const minVoltagePerCell = parseFloat($('minVoltagePerCell').value);
    const maxVoltagePerCell = parseFloat($('maxVoltagePerCell').value);
    const minVoltageTotal = minVoltagePerCell * CELL_COUNT;
    const maxVoltageTotal = maxVoltagePerCell * CELL_COUNT;

    const jbdBmsMac = $('jbdBmsMac').value.trim();
    if ($('jbdBmsEnabled').checked && jbdBmsMac && !/^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$/.test(jbdBmsMac)) {
        showMessage('message', 'BMS MAC must be in format XX:XX:XX:XX:XX:XX', 'err');
        saveButton.disabled = false;
        return;
    }

    const throttleCurveGamma = parseFloat($('throttleCurveGamma').value);
    if (isNaN(throttleCurveGamma) || throttleCurveGamma < 1 || throttleCurveGamma > 3) {
        showMessage('message', 'Throttle curve gamma must be between 1.0 and 3.0', 'err');
        saveButton.disabled = false;
        return;
    }

    const data = {
        batteryCapacity: Math.round(capacityAh * 1000),
        batteryMinVoltage: Math.round(minVoltageTotal * 1000),
        batteryMaxVoltage: Math.round(maxVoltageTotal * 1000),
        motorMaxTemp: Math.round(parseFloat($('motorMaxTemp').value) * 1000),
        motorTempReductionStart: Math.round(parseFloat($('motorTempReductionStart').value) * 1000),
        escMaxTemp: Math.round(parseFloat($('escMaxTemp').value) * 1000),
        escTempReductionStart: Math.round(parseFloat($('escTempReductionStart').value) * 1000),
        powerControlEnabled: $('powerControlEnabled').checked,
        throttleCurveGamma: throttleCurveGamma,
        jbdBmsEnabled: $('jbdBmsEnabled').checked,
        jbdBmsMac: jbdBmsMac,
        wifiAutoDisableAfterCalibration: $('wifiAutoDisableAfterCalibration').checked
    };

    fetch('/config/save', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(data)
    })
        .then((response) => response.text().then((text) => ({ ok: response.ok, text })))
        .then(({ ok, text }) => {
            if (text.includes('Success') || ok) {
                showMessage('message', 'Configuration saved successfully!', 'ok');
            } else {
                showMessage('message', 'Error saving configuration: ' + text, 'err');
            }
            saveButton.disabled = false;
        })
        .catch((error) => {
            showMessage('message', 'Error saving configuration: ' + error, 'err');
            saveButton.disabled = false;
        });
});

loadCurrentValues();
)rawliteral";

    PageSpec spec = {
        "FlyController - Configuration",
        "/config",
        body,
        nullptr,
        script,
        nullptr
    };

    return renderPage(spec);
}
