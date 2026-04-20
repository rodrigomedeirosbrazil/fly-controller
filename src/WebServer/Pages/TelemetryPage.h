#pragma once

#include <Arduino.h>

static const char TELEMETRY_PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>FlyController - Telemetria</title>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
    <meta name="theme-color" content="#0b74de">
    <meta name="mobile-web-app-capable" content="yes">
    <meta name="apple-mobile-web-app-capable" content="yes">
    <link rel="stylesheet" href="/config.css">
</head>
<body class="telemetry-page">
    <div class="page page-telemetry">
        <div class="topbar">
            <a class="nav-btn" href="/">Painel</a>
            <a class="nav-btn active" href="/telemetry">Telemetria</a>
            <a class="nav-btn" href="/firmware">Firmware</a>
            <a class="nav-btn" href="/logs-page">Registros</a>
            <a class="nav-btn" href="/config">Configurações</a>
        </div>

        <div class="telemetry-shell">
            <div class="telemetry-status-bar">
                <span class="tsb-label">Telemetria</span>
                <div class="tsb-mid">
                    <span id="statusBadge" class="status nodata">SEM DADOS</span>
                </div>
                <div class="tsb-right">
                    <button type="button" id="wakeIconBtn" class="wake-icon-btn" aria-expanded="false" aria-controls="wakePanel">&#x1F512;</button>
                </div>
            </div>

            <div class="wake-panel" id="wakePanel">
                <span id="wakeStatusBadge" class="status status-secondary">INATIVO</span>
                <div class="wake-panel-row">
                    <button type="button" id="wakeToggleButton" class="btn btn-sm">Manter Ativo</button>
                </div>
                <div id="wakeHelp" style="margin-top:8px;font-size:12px;line-height:1.35;">A página tentará automaticamente primeiro. Se a tela ainda apagar, toque no botão uma vez.</div>
            </div>

            <div class="grid telemetry-grid">
                <div class="card">
                    <div class="label">Tensão da Bateria</div>
                    <div class="value" id="batteryVoltage">--</div>
                </div>
                <div class="card">
                    <div class="label">SoC da Bateria</div>
                    <div class="value" id="soc">--</div>
                    <div class="sub" id="socCc">--</div>
                </div>
                <div class="card">
                    <div class="label">Energia</div>
                    <div class="value" id="powerKw">--</div>
                    <div class="sub" id="powerPercent">--</div>
                </div>
                <div class="card">
                    <div class="label">Acelerador</div>
                    <div class="value" id="throttlePercent">--</div>
                    <div class="sub" id="throttleRaw">--</div>
                </div>
                <div class="card">
                    <div class="label">Motor</div>
                    <div class="value" id="motorTemp">--</div>
                    <div class="sub" id="rpm">--</div>
                </div>
                <div class="card">
                    <div class="label">ESC</div>
                    <div class="value" id="escTemp">--</div>
                    <div class="sub" id="escCurrent">--</div>
                </div>
                <div class="card">
                    <div class="label">Sistema</div>
                    <div class="value" id="armed">--</div>
                    <div class="sub" id="freshness">--</div>
                </div>
                <div class="card bms-card" id="bmsCard" style="display: none;">
                    <div class="label">BMS</div>
                    <div class="value bms-value" id="bmsTempMax">--</div>
                    <div class="sub multiline bms-meta" id="bmsCells">--</div>
                </div>
            </div>
        </div>
    </div>

    <script defer src="/telemetry.js"></script>
</body>
</html>
)rawliteral";

static const char TELEMETRY_PAGE_JS[] PROGMEM = R"rawliteral(
const $ = (id) => document.getElementById(id);

const setText = (id, value) => {
    const el = $(id);
    if (!el) return;
    if (el.textContent !== value) {
        el.textContent = value;
    }
};

const setHtml = (id, value) => {
    const el = $(id);
    if (!el) return;
    if (el.innerHTML !== value) {
        el.innerHTML = value;
    }
};

const fetchJson = (url) => fetch(url).then((response) => response.json());

const fmtC = (mc) => `${(mc / 1000).toFixed(1)} C`;
const fmtV = (mv) => `${(mv / 1000).toFixed(2)} V`;
const fmtA = (ma) => `${(ma / 1000).toFixed(1)} A`;
const fmtKw = (kwx10) => `${(kwx10 / 10).toFixed(1)} kW`;
const isAppleMobile = /iPhone|iPad|iPod/i.test(navigator.userAgent)
    || (navigator.platform === 'MacIntel' && navigator.maxTouchPoints > 1);

let wakeLockSentinel = null;
let wakeState = 'idle';
let wakeReason = 'Preparing keep-awake controls.';
let wakeDesired = true;
let wakeReacquireTimer = null;

class CanvasWakeFallback {
    constructor() {
        this.video = null;
        this.canvas = null;
        this.context = null;
        this.stream = null;
        this.refreshTimer = null;
        this.frameTick = 0;
    }

    isSupported() {
        const canvas = document.createElement('canvas');
        return typeof canvas.captureStream === 'function' && typeof document.createElement('video').play === 'function';
    }

    drawFrame() {
        if (!this.context || !this.canvas) return;
        this.frameTick += 1;
        this.context.fillStyle = (this.frameTick % 2 === 0) ? '#010101' : '#020202';
        this.context.fillRect(0, 0, this.canvas.width, this.canvas.height);
        this.context.fillStyle = '#000000';
        this.context.fillRect(0, 0, 1, 1);
    }

    async start() {
        if (this.video) {
            await this.video.play();
            return true;
        }

        this.canvas = document.createElement('canvas');
        this.canvas.width = 2;
        this.canvas.height = 2;
        this.context = this.canvas.getContext('2d');
        if (!this.context || typeof this.canvas.captureStream !== 'function') {
            throw new Error('Fallback stream is not available on this browser.');
        }

        this.drawFrame();
        this.stream = this.canvas.captureStream(1);
        this.video = document.createElement('video');
        this.video.muted = true;
        this.video.playsInline = true;
        this.video.loop = true;
        this.video.autoplay = true;
        this.video.setAttribute('muted', '');
        this.video.setAttribute('playsinline', '');
        this.video.style.position = 'fixed';
        this.video.style.width = '1px';
        this.video.style.height = '1px';
        this.video.style.opacity = '0.01';
        this.video.style.pointerEvents = 'none';
        this.video.style.bottom = '0';
        this.video.style.left = '0';
        this.video.srcObject = this.stream;
        document.body.appendChild(this.video);

        this.refreshTimer = window.setInterval(() => this.drawFrame(), 15000);
        await this.video.play();
        return true;
    }

    stop() {
        if (this.refreshTimer) {
            clearInterval(this.refreshTimer);
            this.refreshTimer = null;
        }
        if (this.video) {
            this.video.pause();
            this.video.remove();
            this.video.srcObject = null;
            this.video = null;
        }
        if (this.stream) {
            this.stream.getTracks().forEach((track) => track.stop());
            this.stream = null;
        }
        this.canvas = null;
        this.context = null;
    }
}

const fallbackWake = new CanvasWakeFallback();

const clearWakeReacquireTimer = () => {
    if (wakeReacquireTimer) {
        clearTimeout(wakeReacquireTimer);
        wakeReacquireTimer = null;
    }
};

const scheduleWakeReacquire = (delayMs = 500) => {
    clearWakeReacquireTimer();
    wakeReacquireTimer = window.setTimeout(() => {
        wakeReacquireTimer = null;
        reacquireWakeIfNeeded();
    }, delayMs);
};

const syncWakeUi = (state, reason) => {
    wakeState = state;
    wakeReason = reason || wakeReason;

    const badge = $('wakeStatusBadge');
    const button = $('wakeToggleButton');

    const stateMap = {
        idle: { label: 'INATIVO', cls: 'status-secondary', button: 'Manter Ativo' },
        requesting: { label: 'TENTANDO', cls: 'status-secondary', button: 'Processando...' },
        'active-native': { label: 'ATIVO', cls: 'status-active', button: 'Desativar' },
        'active-fallback': { label: 'ATIVO', cls: 'status-active', button: 'Desativar' },
        'needs-user-gesture': { label: 'TOQUE NECESSÁRIO', cls: 'status-warning', button: 'Manter Ativo' },
        unsupported: { label: 'NÃO SUPORTADO', cls: 'status-inactive', button: 'Tentar Novamente' },
        error: { label: 'TENTAR NOVAMENTE', cls: 'status-warning', button: 'Tentar Novamente' }
    };

    const view = stateMap[state] || stateMap.idle;
    if (badge) {
        badge.className = `status ${view.cls}`;
        badge.textContent = view.label;
    }
    if (button) {
        button.textContent = view.button;
        button.disabled = state === 'requesting';
    }

    if (isAppleMobile) {
        setText(
            'wakeHelp',
            state === 'active-native' || state === 'active-fallback'
                ? 'Mantenha esta página aberta durante o voo. Se o iPhone apagar a tela novamente, volte aqui e toque no botão.'
                : 'O iPhone pode precisar de um toque para manter a tela ativa enquanto esta página permanece aberta.'
        );
    } else {
        setText(
            'wakeHelp',
            state === 'active-native' || state === 'active-fallback'
                ? 'Mantenha esta página visível durante o voo. Se o navegador perder o bloqueio, a página tentará restaurá-lo.'
                : 'A página tentará automaticamente primeiro. Se a tela ainda apagar, toque no botão uma vez.'
        );
    }
};

const stopFallbackWake = () => {
    fallbackWake.stop();
};

const releaseWake = async (markDesired = false) => {
    clearWakeReacquireTimer();

    if (markDesired) {
        wakeDesired = false;
    }

    if (wakeLockSentinel) {
        try {
            await wakeLockSentinel.release();
        } catch (error) {
            // Browsers may throw if the sentinel was already released.
        }
        wakeLockSentinel = null;
    }

    stopFallbackWake();
    syncWakeUi('idle', 'Manter tela ativa está desligado.');
};

const attachWakeLockReleaseHandler = (sentinel) => {
    if (!sentinel || typeof sentinel.addEventListener !== 'function') return;
    sentinel.addEventListener('release', () => {
        wakeLockSentinel = null;
        if (wakeDesired && document.visibilityState === 'visible') {
            syncWakeUi('needs-user-gesture', 'O navegador liberou o bloqueio de tela. Toque no botão se não voltar.');
            scheduleWakeReacquire(300);
        } else {
            syncWakeUi('idle', 'Manter tela ativa está desligado.');
        }
    });
};

const tryNativeWakeLock = async () => {
    if (!('wakeLock' in navigator) || !navigator.wakeLock || typeof navigator.wakeLock.request !== 'function') {
        throw new Error('Screen Wake Lock API is not available.');
    }
    if (document.visibilityState !== 'visible') {
        throw new Error('The page must be visible before requesting wake lock.');
    }

    const sentinel = await navigator.wakeLock.request('screen');
    wakeLockSentinel = sentinel;
    attachWakeLockReleaseHandler(sentinel);
    syncWakeUi('active-native', 'Screen wake lock is active.');
    return true;
};

const tryFallbackWakeLock = async () => {
    if (!fallbackWake.isSupported()) {
        throw new Error('Fallback keep-awake is not supported on this browser.');
    }
    await fallbackWake.start();
    syncWakeUi('active-fallback', isAppleMobile
        ? 'Manter ativo por fallback está ativo para iPhone.'
        : 'Manter ativo por fallback está ativo.');
    return true;
};

const isUserGestureError = (error) => {
    if (!error) return false;
    const message = `${error.name || ''} ${error.message || ''}`.toLowerCase();
    return message.includes('gesture')
        || message.includes('user activation')
        || message.includes('interaction')
        || message.includes('notallowed');
};

const tryAutoEnableWake = async () => {
    wakeDesired = true;

    if (document.visibilityState !== 'visible') {
        syncWakeUi('idle', 'Abra a página para ativar manter ativo.');
        return false;
    }

    clearWakeReacquireTimer();
    syncWakeUi('requesting', 'Tentando manter ativo automaticamente...');

    try {
        await tryNativeWakeLock();
        return true;
    } catch (error) {
        wakeLockSentinel = null;
        stopFallbackWake();

        if (fallbackWake.isSupported()) {
            syncWakeUi(
                'needs-user-gesture',
                isUserGestureError(error)
                    ? 'Este navegador precisa de um toque antes de iniciar manter ativo.'
                    : 'Manter ativo automático é limitado aqui. Toque no botão para iniciar o fallback local.'
            );
            return false;
        }

        syncWakeUi(
            'unsupported',
            'Este navegador não expõe um método de manter ativo no modo atual.'
        );
        return false;
    }
};

const enableWakeFromUserGesture = async () => {
    wakeDesired = true;
    clearWakeReacquireTimer();
    syncWakeUi('requesting', 'Iniciando manter ativo...');

    try {
        await tryNativeWakeLock();
        return true;
    } catch (nativeError) {
        wakeLockSentinel = null;
        try {
            await tryFallbackWakeLock();
            return true;
        } catch (fallbackError) {
            stopFallbackWake();
            syncWakeUi(
                'error',
                isUserGestureError(fallbackError) || isUserGestureError(nativeError)
                    ? 'O navegador ainda requer um toque direto. Tente o botão novamente sem sair da página.'
                    : 'Não é possível manter a tela ativa neste navegador.'
            );
            return false;
        }
    }
};

async function reacquireWakeIfNeeded() {
    if (!wakeDesired || document.visibilityState !== 'visible') {
        return;
    }

    if (wakeLockSentinel || wakeState === 'active-fallback') {
        return;
    }

    await tryAutoEnableWake();
}

const initTelemetryWake = () => {
    syncWakeUi('idle', 'Preparando controles de manter ativo.');

    const helpToggle = $('wakeHelpToggle');
    const helpPanel = $('wakeHelp');
    if (helpToggle && helpPanel) {
        helpToggle.addEventListener('click', () => {
            const isOpen = helpPanel.classList.toggle('open');
            helpToggle.setAttribute('aria-expanded', isOpen ? 'true' : 'false');
        });
    }

    const button = $('wakeToggleButton');
    if (button) {
        button.addEventListener('click', async () => {
            if (wakeState === 'active-native' || wakeState === 'active-fallback') {
                await releaseWake(true);
                return;
            }
            await enableWakeFromUserGesture();
        });
    }

    document.addEventListener('visibilitychange', async () => {
        if (document.visibilityState === 'visible') {
            scheduleWakeReacquire(250);
            return;
        }

        if (wakeLockSentinel) {
            try {
                await wakeLockSentinel.release();
            } catch (error) {
                // Ignore duplicate release errors while backgrounding the tab.
            }
            wakeLockSentinel = null;
        }

        if (wakeState === 'active-fallback') {
            stopFallbackWake();
            syncWakeUi('needs-user-gesture', 'Volte a esta página e toque no botão se a tela começar a apagar novamente.');
        }
    });

    window.addEventListener('pageshow', () => scheduleWakeReacquire(250));
    window.addEventListener('focus', () => scheduleWakeReacquire(250));
    window.addEventListener('pagehide', () => {
        clearWakeReacquireTimer();
        if (wakeState === 'active-fallback') {
            stopFallbackWake();
        }
    });
    window.addEventListener('beforeunload', () => {
        clearWakeReacquireTimer();
        stopFallbackWake();
    });

    tryAutoEnableWake();
};

const setStatus = (kind) => {
    const badge = $('statusBadge');
    if (!badge) return;
    badge.className = `status ${kind}`;
    badge.textContent = kind === 'live' ? 'AO VIVO' : (kind === 'stale' ? 'DESATUALIZADO' : 'SEM DADOS');
};

const renderTelemetry = (data) => {
    const av = data.availability || {};

    if (!data.hasTelemetry) {
        setStatus('nodata');
        setText('freshness', 'Aguardando telemetria');
    } else {
        const age = data.uptimeMs - data.lastTelemetryUpdateMs;
        setStatus(age > 3000 ? 'stale' : 'live');
        setText('freshness', `Última atualização há ${Math.max(0, age)} ms`);
    }

    setText('batteryVoltage', fmtV(data.batteryVoltageMv || 0));
    setText('soc', `${data.batteryPercentVoltage || 0} %`);
    setText('socCc', `CC: ${data.batteryPercentCc ?? 0} %`);
    setText('powerKw', av.powerKw ? fmtKw(data.powerKwX10 ?? 0) : 'N/A');
    setText('powerPercent', `Limite: ${data.powerPercent || 0} %`);
    setText('throttlePercent', `${data.throttlePercent || 0} %`);
    setText('throttleRaw', `Bruto: ${data.throttleRaw || 0}`);
    setText('motorTemp', fmtC(data.motorTempMc || 0));
    setText('rpm', av.rpm ? `${data.rpm ?? 0} rpm` : 'N/A');
    setText('escTemp', fmtC(data.escTempMc || 0));
    setText('escCurrent', av.current ? fmtA(data.escCurrentMa ?? 0) : 'N/A');
    setText('armed', data.armed ? 'ARMADO' : 'DESARMADO');

    const bmsCard = $('bmsCard');
    if (data.bms && data.bms.available) {
        bmsCard.style.display = '';
        setText('bmsTempMax', data.bms.tempMaxC != null ? `${data.bms.tempMaxC} C` : '--');
        if (data.bms.cellMinMv != null && data.bms.cellMaxMv != null) {
            setHtml('bmsCells', `Cell: ${data.bms.cellMinMv}-${data.bms.cellMaxMv} mV<br>Delta: ${data.bms.cellDeltaMv ?? '--'} mV`);
        } else {
            setText('bmsCells', '--');
        }
    } else {
        bmsCard.style.display = 'none';
    }
};

const loadTelemetry = () => {
    fetchJson('/api/telemetry')
        .then(renderTelemetry)
        .catch(() => setStatus('nodata'));
};

document.addEventListener('DOMContentLoaded', () => {
    initTelemetryWake();
    loadTelemetry();
    setInterval(loadTelemetry, 1000);
});
)rawliteral";
