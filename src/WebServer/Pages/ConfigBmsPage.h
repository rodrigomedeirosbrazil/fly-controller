#pragma once

#include <Arduino.h>

static const char CONFIG_BMS_PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>FlyController - BMS Bluetooth</title>
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
            <a class="nav-btn active" href="/config/bms">BMS</a>
            <a class="nav-btn" href="/config/system">Sistema</a>
        </div>

        <div class="panel">
            <h1>BMS Bluetooth</h1>

            <form id="bmsConfigForm">
                <div class="form-group">
                    <label for="bmsType">Tipo de BMS:</label>
                    <select id="bmsType" name="bmsType">
                        <option value="0">Desativado</option>
                        <option value="1">JBD</option>
                        <option value="2">Daly (D2 BLE)</option>
                    </select>
                    <div class="info-text">Selecione o backend BMS Bluetooth usado pelo controlador.</div>
                </div>

                <div class="form-group">
                    <label for="bmsMac">Endereço Bluetooth do BMS (MAC):</label>
                    <input type="text" id="bmsMac" name="bmsMac" maxlength="17" placeholder="A5:C2:39:2B:FC:4E">
                    <div class="info-text">Formato: XX:XX:XX:XX:XX:XX (6 bytes hex com dois-pontos).</div>
                </div>

                <div class="form-group">
                    <button type="button" id="scanBmsButton">Buscar BMS</button>
                    <div class="info-text" id="bmsScanStatus">Pressione "Buscar BMS" para procurar dispositivos JBD e Daly próximos.</div>
                    <div id="bmsScanResults" style="display: grid; gap: 10px; margin-top: 12px;"></div>
                </div>

                <div class="form-group">
                    <label for="configPin">PIN</label>
                    <input type="password" id="configPin" maxlength="8" placeholder="Necessário para salvar">
                </div>

                <button type="submit" id="saveButton">Salvar Configurações do BMS</button>
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
    0: 'Desconhecido',
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
            <div>${escapeHtml(entry.name || 'Dispositivo sem nome')}</div>
            <div>MAC: <code>${escapeHtml(entry.mac || '')}</code></div>
            <div>RSSI: ${typeof entry.rssi === 'number' ? entry.rssi + ' dBm' : 'N/A'}</div>
            <div>Services: <code>${escapeHtml(entry.advertisedServices || 'none')}</code></div>
            <div><button type="button" class="use-bms-result" data-mac="${escapeHtml(entry.mac || '')}" data-type="${escapeHtml(entry.detectedType || 0)}">Usar este BMS</button></div>
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
                setBmsScanStatus('BMS escaneado selecionado. Revise o tipo detectado e salve a configuração.');
            } else {
                $('scanBmsButton').disabled = true;
                setBmsScanStatus('Tentando detectar o tipo de BMS conectando ao dispositivo selecionado...');
                detectBmsType(selectedMac)
                    .then((data) => {
                        $('scanBmsButton').disabled = false;
                        if (data && Number(data.detectedType) > 0) {
                            $('bmsType').value = String(data.detectedType);
                            setBmsScanStatus('Tipo de BMS detectado após conexão. Revise os campos e salve a configuração.');
                        } else {
                            setBmsScanStatus('MAC do dispositivo BLE selecionado. Tipo ainda desconhecido após o teste de conexão.');
                        }
                    })
                    .catch((error) => {
                        console.error('Error detecting BMS type:', error);
                        $('scanBmsButton').disabled = false;
                        setBmsScanStatus('Não foi possível detectar o tipo de BMS após o teste de conexão.');
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
        setBmsScanStatus('Buscando dispositivos BLE próximos...');
        renderBmsScanResults(Array.isArray(data.results) ? data.results : []);
        scheduleBmsScanPolling();
        return;
    }

    stopBmsScanPolling();

    if (status === 'error') {
        setBmsScanStatus(data && data.error ? `Busca falhou: ${data.error}` : 'Busca falhou.');
        renderBmsScanResults([]);
        return;
    }

    if (status === 'complete') {
        const results = Array.isArray(data.results) ? data.results : [];
        renderBmsScanResults(results);
        if (results.length === 0) {
            setBmsScanStatus('Nenhum dispositivo BLE encontrado nas proximidades.');
        } else {
            setBmsScanStatus('Exibindo todos os dispositivos BLE. Entradas JBD/Daly reconhecidas são rotuladas automaticamente.');
        }
        return;
    }

    renderBmsScanResults(Array.isArray(data && data.results) ? data.results : []);
    setBmsScanStatus('Pressione "Buscar BMS" para procurar dispositivos BLE próximos e inspecionar os serviços anunciados.');
};

function loadBmsScanStatus() {
    fetch('/api/bms/scan/status')
        .then((response) => response.json())
        .then(applyBmsScanState)
        .catch((error) => {
            console.error('Error loading BMS scan status:', error);
            stopBmsScanPolling();
            $('scanBmsButton').disabled = false;
            setBmsScanStatus('Não foi possível ler o status da busca BLE.');
        });
}

const startBmsScan = () => {
    $('scanBmsButton').disabled = true;
    setBmsScanStatus('Iniciando busca BLE...');
    fetch('/api/bms/scan/start', { method: 'POST', headers: { 'X-Config-Pin': getPin() } })
        .then((response) => response.json())
        .then(applyBmsScanState)
        .catch((error) => {
            console.error('Error starting BMS scan:', error);
            $('scanBmsButton').disabled = false;
            setBmsScanStatus('Não foi possível iniciar a busca BLE.');
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
            showMessage('Erro ao carregar a configuração atual', 'err');
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
        showMessage('O MAC do BMS deve estar no formato XX:XX:XX:XX:XX:XX', 'err');
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
            showMessage(ok ? 'Configurações do BMS salvas com sucesso!' : 'Erro ao salvar a configuração: ' + text, ok ? 'ok' : 'err');
            saveButton.disabled = false;
        })
        .catch((error) => {
            showMessage('Erro ao salvar a configuração: ' + error, 'err');
            saveButton.disabled = false;
        });
});

window.addEventListener('DOMContentLoaded', () => { $('configPin').value = getPin(); });

loadCurrentValues();
loadBmsScanStatus();
)rawliteral";
