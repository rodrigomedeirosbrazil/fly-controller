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

                <div class="form-group">
                    <button type="button" id="scanBmsButton">Scan for BMS</button>
                    <div class="info-text" id="bmsScanStatus">Press "Scan for BMS" to search nearby JBD and Daly devices.</div>
                    <div id="bmsScanResults" style="display: grid; gap: 10px; margin-top: 12px;"></div>
                </div>

                <div class="form-group">
                    <label for="configPin">PIN</label>
                    <input type="password" id="configPin" maxlength="8" placeholder="Required to save">
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
const getPin = () => sessionStorage.getItem('cfgPin') || '';
const setPin = (v) => sessionStorage.setItem('cfgPin', v);
const BMS_TYPE_LABELS = {
    0: 'Unknown',
    1: 'JBD',
    2: 'Daly (D2 BLE)'
};

let bmsScanPollTimer = null;

const showMessage = (text, kind) => {
    const el = $('message');
    if (!el) return;
    el.textContent = text;
    el.className = `message ${kind}`;
    el.style.display = 'block';
};

const escapeHtml = (value) => String(value || '')
    .replace(/&/g, '&amp;')
    .replace(/[\u003C]/g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;');

const setBmsScanStatus = (text) => {
    $('bmsScanStatus').textContent = text;
};

const stopBmsScanPolling = () => {
    if (bmsScanPollTimer) {
        clearTimeout(bmsScanPollTimer);
        bmsScanPollTimer = null;
    }
};

const scheduleBmsScanPolling = (delayMs = 1000) => {
    stopBmsScanPolling();
    bmsScanPollTimer = setTimeout(loadBmsScanStatus, delayMs);
};

const renderBmsScanResults = (results) => {
    const container = $('bmsScanResults');
    container.innerHTML = '';

    if (!Array.isArray(results) || results.length === 0) {
        return;
    }

    const sortedResults = [...results].sort((a, b) => (b.rssi || -999) - (a.rssi || -999));
    sortedResults.forEach((entry) => {
        const row = document.createElement('div');
        row.style.border = '1px solid rgba(255,255,255,0.12)';
        row.style.borderRadius = '10px';
        row.style.padding = '12px';
        row.style.display = 'grid';
        row.style.gap = '8px';
        row.innerHTML = `
            <div><strong>${escapeHtml(BMS_TYPE_LABELS[entry.detectedType] || 'Unknown')}</strong></div>
            <div>${escapeHtml(entry.name || 'Unnamed device')}</div>
            <div>MAC: <code>${escapeHtml(entry.mac || '')}</code></div>
            <div>RSSI: ${typeof entry.rssi === 'number' ? entry.rssi + ' dBm' : 'N/A'}</div>
            <div>Services: <code>${escapeHtml(entry.advertisedServices || 'none')}</code></div>
            <div><button type="button" class="use-bms-result" data-mac="${escapeHtml(entry.mac || '')}" data-type="${escapeHtml(entry.detectedType || 0)}">Use this BMS</button></div>
        `;
        container.appendChild(row);
    });

    container.querySelectorAll('.use-bms-result').forEach((button) => {
        button.addEventListener('click', () => {
            const selectedMac = button.dataset.mac || '';
            const detectedType = button.dataset.type || '0';

            $('bmsMac').value = selectedMac;
            if (detectedType !== '0') {
                $('bmsType').value = detectedType;
                setBmsScanStatus('Selected scanned BMS. Review the detected type and save the configuration.');
            } else {
                $('scanBmsButton').disabled = true;
                setBmsScanStatus('Trying to detect the BMS type by connecting to the selected device...');
                detectBmsType(selectedMac)
                    .then((data) => {
                        $('scanBmsButton').disabled = false;
                        if (data && Number(data.detectedType) > 0) {
                            $('bmsType').value = String(data.detectedType);
                            setBmsScanStatus('BMS type detected after connection. Review the fields and save the configuration.');
                        } else {
                            setBmsScanStatus('Selected BLE device MAC. Type is still unknown after the connection test.');
                        }
                    })
                    .catch((error) => {
                        console.error('Error detecting BMS type:', error);
                        $('scanBmsButton').disabled = false;
                        setBmsScanStatus('Could not detect the BMS type after the connection test.');
                    });
            }
        });
    });
};

const applyBmsScanState = (data) => {
    const status = data && data.status ? data.status : 'idle';
    const isBusy = !!(data && data.busy);
    $('scanBmsButton').disabled = isBusy;

    if (status === 'scanning') {
        setBmsScanStatus('Scanning nearby BLE devices...');
        renderBmsScanResults(Array.isArray(data.results) ? data.results : []);
        scheduleBmsScanPolling();
        return;
    }

    stopBmsScanPolling();

    if (status === 'error') {
        setBmsScanStatus(data && data.error ? `Scan failed: ${data.error}` : 'Scan failed.');
        renderBmsScanResults([]);
        return;
    }

    if (status === 'complete') {
        const results = Array.isArray(data.results) ? data.results : [];
        renderBmsScanResults(results);
        if (results.length === 0) {
            setBmsScanStatus('No BLE devices were found nearby.');
        } else {
            setBmsScanStatus('Showing all BLE devices. Recognized JBD/Daly entries are labeled automatically.');
        }
        return;
    }

    renderBmsScanResults(Array.isArray(data && data.results) ? data.results : []);
    setBmsScanStatus('Press "Scan for BMS" to search nearby BLE devices and inspect advertised services.');
};

function loadBmsScanStatus() {
    fetch('/api/bms/scan/status')
        .then((response) => response.json())
        .then(applyBmsScanState)
        .catch((error) => {
            console.error('Error loading BMS scan status:', error);
            stopBmsScanPolling();
            $('scanBmsButton').disabled = false;
            setBmsScanStatus('Unable to read BLE scan status.');
        });
}

const startBmsScan = () => {
    $('scanBmsButton').disabled = true;
    setBmsScanStatus('Starting BLE scan...');
    fetch('/api/bms/scan/start', { method: 'POST', headers: { 'X-Config-Pin': getPin() } })
        .then((response) => response.json())
        .then(applyBmsScanState)
        .catch((error) => {
            console.error('Error starting BMS scan:', error);
            $('scanBmsButton').disabled = false;
            setBmsScanStatus('Unable to start BLE scan.');
        });
};

const detectBmsType = (mac) => {
    return fetch('/api/bms/detect', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ mac })
    }).then((response) => response.json());
};

const loadCurrentValues = () => {
    fetch('/api/config/bms')
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

$('scanBmsButton').addEventListener('click', startBmsScan);

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

    const pin = $('configPin').value;
    setPin(pin);

    fetch('/api/config/bms', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json', 'X-Config-Pin': pin },
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

window.addEventListener('DOMContentLoaded', () => { $('configPin').value = getPin(); });

loadCurrentValues();
loadBmsScanStatus();
)rawliteral";
