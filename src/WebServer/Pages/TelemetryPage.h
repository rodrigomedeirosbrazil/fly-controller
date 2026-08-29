#pragma once

#include <Arduino.h>

static const char TELEMETRY_PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>FlyController - Telemetria</title>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
    <meta name="theme-color" content="#0a0e13">
    <meta name="mobile-web-app-capable" content="yes">
    <meta name="apple-mobile-web-app-capable" content="yes">
    <link rel="stylesheet" href="/config.css">
</head>
<body class="telemetry-page">
    <div class="tp">
        <div class="tp-main">

            <div class="tp-status">
                <span class="tp-pill danger" id="statusPill"><span class="tp-dot"></span><span id="statusLabel">SEM DADOS</span></span>
                <span class="tp-pill off" id="armedPill"><span class="tp-dot"></span><span id="armedLabel">DESARMADO</span></span>
                <button type="button" class="tp-pill alarm" id="faultChip" style="display:none;">
                    <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.4" stroke-linecap="round" stroke-linejoin="round"><path d="M12 3 2 20h20L12 3Z"/><path d="M12 10v4"/><path d="M12 17.5v.5"/></svg>
                    <span id="faultCode"></span>
                </button>
                <span class="tp-spacer"></span>
                <span class="tp-clock" id="sessionTime">0:00:00</span>
                <button type="button" class="tp-icon off" id="soundBtn" aria-label="Ativar som">
                    <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M11 5 6 9H3v6h3l5 4V5Z"/><path d="M16.5 8.5a5 5 0 0 1 0 7"/></svg>
                </button>
                <button type="button" class="tp-icon" id="wakeBtn" aria-label="Manter tela ativa">
                    <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="4" y="10" width="16" height="10" rx="2"/><path d="M8 10V7a4 4 0 0 1 8 0v3"/></svg>
                </button>
            </div>

            <div class="tp-tile tp-hero" id="tileBattery">
                <svg class="tp-gauge tp-gauge-batt" viewBox="0 0 240 186" xmlns="http://www.w3.org/2000/svg">
                    <g transform="rotate(135 120 120)">
                        <circle class="g-track" cx="120" cy="120" r="80" stroke-width="16" stroke-dasharray="376.99 502.65"></circle>
                        <circle class="g-zone" cx="120" cy="120" r="80" stroke-width="16" stroke-dasharray="75.40 502.65"></circle>
                        <circle class="g-val" id="battArc" cx="120" cy="120" r="80" stroke-width="16" stroke-dasharray="0 502.65"></circle>
                    </g>
                    <text class="g-num" id="soc" x="120" y="124" text-anchor="middle" font-size="80">--</text>
                    <text class="g-cap" x="120" y="152" text-anchor="middle" font-size="15">BATERIA %</text>
                    <text class="g-end" x="24" y="180" font-size="13">0</text>
                    <text class="g-end" x="216" y="180" text-anchor="end" font-size="13">100</text>
                </svg>
                <div class="tp-sep"></div>
                <div class="tp-cells" id="heroCells">
                    <button type="button" class="tp-cell tp-cell-btn" id="voltageCell" aria-label="Alternar entre tens&#227;o total e por c&#233;lula">
                        <div class="tp-lab">Tens&#227;o
                            <svg width="9" height="9" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="3" stroke-linecap="round" stroke-linejoin="round"><path d="M7 4 3 8l4 4"/><path d="M3 8h13"/><path d="m17 20 4-4-4-4"/><path d="M21 16H8"/></svg>
                        </div>
                        <div class="tp-num"><span id="voltageValue">--</span><span class="tp-unit" id="voltageUnit"> V</span></div>
                    </button>
                    <div class="tp-cell" id="cellCurrent">
                        <div class="tp-lab">Corrente</div>
                        <div class="tp-num"><span id="packCurrent">--</span><span class="tp-unit"> A</span></div>
                    </div>
                </div>
            </div>

            <div class="tp-instruments" id="instruments">
                <div class="tp-tile tp-inst" id="tilePower">
                    <span class="tp-lab">Pot&#234;ncia</span>
                    <span class="tp-num" id="powerKw">--</span>
                    <span class="tp-unit">kW</span>
                    <span class="tp-chip" id="powerAvail" style="display:none;"></span>
                </div>
                <div class="tp-tile tp-inst" id="tileMotor">
                    <svg class="tp-gauge" viewBox="0 0 130 108" xmlns="http://www.w3.org/2000/svg">
                        <g transform="rotate(135 65 65)">
                            <circle class="g-track" cx="65" cy="65" r="46" stroke-width="11" stroke-dasharray="216.77 289.03"></circle>
                            <circle class="g-zone" id="motorZone" cx="65" cy="65" r="46" stroke-width="11" stroke-dasharray="0 289.03"></circle>
                            <circle class="g-val" id="motorArc" cx="65" cy="65" r="46" stroke-width="11" stroke-dasharray="0 289.03"></circle>
                        </g>
                        <text class="g-num" id="motorTemp" x="65" y="62" text-anchor="middle" font-size="34">--</text>
                        <text class="g-unit" x="65" y="82" text-anchor="middle" font-size="13">&#176;C</text>
                        <text class="g-sensor" id="motorSensor" x="65" y="99" text-anchor="middle" font-size="10"></text>
                    </svg>
                    <span class="tp-lab">Motor</span>
                </div>
                <div class="tp-tile tp-inst" id="tileEsc">
                    <svg class="tp-gauge" viewBox="0 0 130 108" xmlns="http://www.w3.org/2000/svg">
                        <g transform="rotate(135 65 65)">
                            <circle class="g-track" cx="65" cy="65" r="46" stroke-width="11" stroke-dasharray="216.77 289.03"></circle>
                            <circle class="g-zone" id="escZone" cx="65" cy="65" r="46" stroke-width="11" stroke-dasharray="0 289.03"></circle>
                            <circle class="g-val" id="escArc" cx="65" cy="65" r="46" stroke-width="11" stroke-dasharray="0 289.03"></circle>
                        </g>
                        <text class="g-num" id="escTemp" x="65" y="62" text-anchor="middle" font-size="34">--</text>
                        <text class="g-unit" x="65" y="82" text-anchor="middle" font-size="13">&#176;C</text>
                    </svg>
                    <span class="tp-lab">ESC</span>
                </div>
            </div>

            <div class="tp-tile tp-throttle">
                <span class="tp-lab">Acelerador</span>
                <span class="tp-num tp-thr-val"><span id="throttlePercent">--</span> %</span>
                <div class="tp-bar"><i id="throttleFill" style="width:0%"></i><i class="tp-cap" id="throttleCap"></i></div>
            </div>

            <button type="button" class="tp-more" id="moreBtn">
                <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round" style="color:var(--muted);flex-shrink:0;"><path d="m6 15 6-6 6 6"/></svg>
                <span class="tp-lab">Mais dados</span>
                <span class="tp-spacer"></span>
                <span class="tp-lab tp-more-hint">RPM &#183; BMS &#183; c&#233;lulas &#183; leitura</span>
            </button>
        </div>

        <nav class="tp-nav">
            <a href="/">Painel</a>
            <a class="active" href="/telemetry">Telem.</a>
            <a href="/firmware">Firmw.</a>
            <a href="/logs-page">Regist.</a>
            <a href="/config">Config.</a>
        </nav>
    </div>

    <div class="tp-scrim" id="scrim"></div>
    <div class="tp-drawer" id="drawer">
        <div class="tp-grab"></div>
        <div class="tp-drawer-head">
            <span class="tp-lab" style="font-size:13px;color:var(--text);letter-spacing:0.1em;">Mais dados</span>
            <span class="tp-spacer"></span>
            <button type="button" class="tp-icon" id="drawerClose" style="width:34px;height:34px;" aria-label="Fechar">
                <svg width="17" height="17" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round"><path d="m6 9 6 6 6-6"/></svg>
            </button>
        </div>

        <div class="tp-fault" id="faultBlock" style="display:none;">
            <svg width="19" height="19" viewBox="0 0 24 24" fill="none" stroke="var(--danger)" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="flex-shrink:0;"><path d="M12 3 2 20h20L12 3Z"/><path d="M12 10v4"/><path d="M12 17.5v.5"/></svg>
            <div style="min-width:0;">
                <div class="tp-fault-title" id="faultTitle"></div>
                <div class="tp-fault-detail" id="faultDetail"></div>
            </div>
        </div>

        <div class="tp-rows">
            <div class="tp-sec">Motor e ESC</div>
            <div class="tp-row"><span class="k">Rota&#231;&#227;o</span><span class="v" id="rpm">--</span></div>
            <div class="tp-row"><span class="k">Corrente do ESC</span><span class="v" id="escCurrent">--</span></div>
            <div class="tp-row"><span class="k">Sensor da temp. do motor</span><span class="v" id="motorSrcRow">--</span></div>

            <div class="tp-sec">Bateria e BMS</div>
            <div class="tp-row"><span class="k">SoC por tens&#227;o</span><span class="v" id="socVoltage">--</span></div>
            <div class="tp-row"><span class="k">Tens&#227;o total</span><span class="v" id="packVoltage">--</span></div>
            <div class="tp-row"><span class="k">Origem da tens&#227;o</span><span class="v" id="cellSource">--</span></div>
            <div class="tp-row" id="rowBms"><span class="k">BMS</span><span class="v" id="bmsStatus">--</span></div>
            <div class="tp-row" id="rowCells"><span class="k">C&#233;lulas (m&#237;n &#8211; m&#225;x)</span><span class="v" id="bmsCells">--</span></div>
            <div class="tp-row" id="rowDelta"><span class="k">Delta entre c&#233;lulas</span><span class="v" id="bmsDelta">--</span></div>
            <div class="tp-row" id="rowBmsTemp"><span class="k">Temp. m&#225;x do BMS</span><span class="v" id="bmsTempMax">--</span></div>

            <div class="tp-sec">Acelerador e sistema</div>
            <div class="tp-row"><span class="k">Leitura</span><span class="v" id="throttleRaw">--</span></div>
            <div class="tp-row"><span class="k">Hor&#237;metro do motor</span><span class="v" id="hourMeter">--</span></div>
            <div class="tp-row"><span class="k">Manter tela ativa</span><span class="v" id="wakeStateRow">INATIVO</span></div>
            <div class="tp-row"><span class="k" id="wakeHelp" style="font-size:12.5px;line-height:1.35;"></span></div>
        </div>

        <div class="tp-drawer-foot">
            <span class="tp-lab">Tempo de v&#244;o</span>
            <span class="tp-num" style="font-size:20px;" id="drawerSession">0:00:00</span>
            <span class="tp-spacer"></span>
            <button type="button" class="btn btn-sm" id="resetSessionButton">Resetar</button>
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

const fetchJson = (url) => fetch(url).then((response) => response.json());

// Every storage access is guarded. A browser with site data blocked (private
// windows, some WebViews) throws on the property itself, and this script runs
// at top level -- an unguarded read would kill the whole panel over a mute
// preference.
const storeGet = (store, key) => {
    try { return window[store].getItem(key); } catch (e) { return null; }
};

const storeSet = (store, key, value) => {
    try { window[store].setItem(key, value); } catch (e) { /* nothing to do */ }
};

const fmtV = (mv) => `${(mv / 1000).toFixed(2)} V`;
const fmtA = (ma) => `${(ma / 1000).toFixed(1)} A`;
const fmtSeconds = s => {
    const h = Math.floor(s / 3600);
    const m = Math.floor((s % 3600) / 60);
    const sec = s % 60;
    return `${h}:${String(m).padStart(2,'0')}:${String(sec).padStart(2,'0')}`;
};
const isAppleMobile = /iPhone|iPad|iPod/i.test(navigator.userAgent)
    || (navigator.platform === 'MacIntel' && navigator.maxTouchPoints > 1);

// ============ Web Audio Buzzer ============
let audioCtx = null;
let gainMaster = null;
let soundMuted = storeGet('localStorage', 'bzMuted') !== '0';  // default: muted
let bzStopLoopFlag = false;
let bzActiveOsc = null;
let bzActiveGain = null;
let bzLastSeq = -1;
let bzPrimed = false;
let bzLoopIsRunning = false;
let bzStateOn = false;       // is the state-layer (continuous) loop currently playing
let bzStatePattern = null;   // {freq, onMs, offMs} of the active state, for resuming after an event

const bzInitCtx = () => {
    if (audioCtx) return;
    audioCtx = new (window.AudioContext || window.webkitAudioContext)();
    gainMaster = audioCtx.createGain();
    gainMaster.connect(audioCtx.destination);
    gainMaster.gain.value = soundMuted ? 0 : 1;
};

const bzApplyMute = () => {
    if (gainMaster) gainMaster.gain.value = soundMuted ? 0 : 1;
    const btn = $('soundBtn');
    if (!btn) return;
    btn.classList.toggle('off', soundMuted);
    btn.classList.toggle('on', !soundMuted);
    btn.setAttribute('aria-label', soundMuted ? 'Ativar som' : 'Silenciar');
};

const bzStopLoop = () => {
    bzStopLoopFlag = true;
    bzLoopIsRunning = false;
    if (bzActiveOsc) {
        try { bzActiveOsc.stop(); } catch(e) {}
        bzActiveOsc = null;
        bzActiveGain = null;
    }
};

const bzScheduleOnce = (freq, onMs, offMs, reps, startT) => {
    let t = startT;
    for (let i = 0; i < reps; i++) {
        const osc = audioCtx.createOscillator();
        const g = audioCtx.createGain();
        osc.connect(g);
        g.connect(gainMaster);
        osc.type = 'square';
        osc.frequency.value = freq;
        g.gain.setValueAtTime(1, t);
        g.gain.setValueAtTime(0, t + onMs / 1000);
        osc.start(t);
        osc.stop(t + onMs / 1000);
        t += (onMs + offMs) / 1000;
    }
    return t;
};

const bzStartLoop = (freq, onMs, offMs) => {
    if (!audioCtx) return;
    bzStopLoop();
    bzStopLoopFlag = false;
    bzLoopIsRunning = true;
    bzActiveGain = audioCtx.createGain();
    bzActiveGain.connect(gainMaster);
    bzActiveOsc = audioCtx.createOscillator();
    bzActiveOsc.connect(bzActiveGain);
    bzActiveOsc.type = 'square';
    bzActiveOsc.frequency.value = freq;
    bzActiveOsc.start(audioCtx.currentTime);
    const step = async () => {
        while (!bzStopLoopFlag && bzLoopIsRunning) {
            bzActiveGain.gain.setValueAtTime(1, audioCtx.currentTime);
            await new Promise(r => setTimeout(r, onMs));
            if (bzStopLoopFlag || !bzLoopIsRunning) break;
            bzActiveGain.gain.setValueAtTime(0, audioCtx.currentTime);
            await new Promise(r => setTimeout(r, offMs));
        }
        if (bzActiveOsc) {
            try { bzActiveOsc.stop(); } catch(e) {}
            bzActiveOsc = null;
            bzActiveGain = null;
        }
        bzLoopIsRunning = false;
    };
    step();
};

const bzPlayQueue = (events) => {
    if (!audioCtx) return;
    let t = audioCtx.currentTime;
    let interruptedState = false;

    for (const ev of events) {
        if (ev.layer === 1) {
            // State layer: only act on a real transition. A repeated
            // "active:true" for an already-running state (or a repeated
            // "active:false" for an already-stopped one) is ignored so the
            // loop is never restarted mid-cycle.
            if (ev.active && !bzStateOn) {
                bzStartLoop(ev.freq, ev.onMs, ev.offMs);
                bzStateOn = true;
                bzStatePattern = { freq: ev.freq, onMs: ev.onMs, offMs: ev.offMs };
                t = audioCtx.currentTime;
            } else if (!ev.active && bzStateOn) {
                bzStopLoop();
                bzStateOn = false;
                bzStatePattern = null;
                t = audioCtx.currentTime;
            }
            continue;
        }

        // Event layer: a queued, finite pattern. If the state loop is
        // running, pause it so the event can be heard, then resume it
        // after (see below) -- accepting a small timing imprecision on the
        // resume rather than trying to reproduce the firmware's exact
        // sample-accurate preemption in the browser.
        if (bzLoopIsRunning) {
            bzStopLoop();
            interruptedState = true;
            t = audioCtx.currentTime;
        }
        t = bzScheduleOnce(ev.freq, ev.onMs, ev.offMs, ev.reps, t);
    }

    if (bzStateOn && (interruptedState || !bzLoopIsRunning)) {
        const pattern = bzStatePattern;
        const delayMs = Math.max(0, (t - audioCtx.currentTime) * 1000);
        setTimeout(() => {
            if (bzStateOn && !bzLoopIsRunning) bzStartLoop(pattern.freq, pattern.onMs, pattern.offMs);
        }, delayMs);
    }
};

const bzProcessEvents = (events) => {
    if (!bzPrimed) {
        bzPrimed = true;
        if (events && events.length) {
            bzLastSeq = events[events.length - 1].seq;
            // Skip replaying queued (layer 0) history, but apply the last
            // known state (layer 1) so the page starts in sync with
            // whatever the device is already doing -- e.g. opening the
            // page while armed and stopped should start the continuous
            // alert immediately instead of staying silent.
            for (let i = events.length - 1; i >= 0; i--) {
                if (events[i].layer === 1) {
                    bzPlayQueue([events[i]]);
                    break;
                }
            }
        } else {
            bzLastSeq = -1;
        }
        return;
    }
    if (!events || !events.length) return;
    const fresh = events.filter(ev => ev.seq > bzLastSeq);
    if (!fresh.length) return;
    bzLastSeq = fresh[fresh.length - 1].seq;
    bzPlayQueue(fresh);
};

const initBuzzerSound = () => {
    bzApplyMute();
    const btn = $('soundBtn');
    if (!btn) return;
    btn.addEventListener('click', () => {
        bzInitCtx();
        if (audioCtx && audioCtx.state === 'suspended') {
            audioCtx.resume();
        }
        soundMuted = !soundMuted;
        storeSet('localStorage', 'bzMuted', soundMuted ? '1' : '0');
        bzApplyMute();
    });
};
// ============ End Web Audio Buzzer ============

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

const WAKE_STATE_LABEL = {
    idle: 'INATIVO',
    requesting: 'TENTANDO',
    'active-native': 'ATIVO',
    'active-fallback': 'ATIVO',
    'needs-user-gesture': 'TOQUE NECESSÁRIO',
    unsupported: 'NÃO SUPORTADO',
    error: 'TENTAR NOVAMENTE',
};

const wakeIsActive = (state) => state === 'active-native' || state === 'active-fallback';

const syncWakeUi = (state, reason) => {
    wakeState = state;
    wakeReason = reason || wakeReason;

    const btn = $('wakeBtn');
    if (btn) {
        const active = wakeIsActive(state);
        btn.classList.toggle('on', active);
        btn.classList.toggle('off', !active);
        btn.setAttribute('aria-label', active ? 'Desativar manter tela ativa' : 'Manter tela ativa');
    }

    setText('wakeStateRow', WAKE_STATE_LABEL[state] || WAKE_STATE_LABEL.idle);

    if (isAppleMobile) {
        setText(
            'wakeHelp',
            wakeIsActive(state)
                ? 'Mantenha esta página aberta durante o voo. Se o iPhone apagar a tela novamente, volte aqui e toque no cadeado.'
                : 'O iPhone pode precisar de um toque no cadeado para manter a tela ativa.'
        );
    } else {
        setText(
            'wakeHelp',
            wakeIsActive(state)
                ? 'Mantenha esta página visível durante o voo. Se o navegador perder o bloqueio, a página tentará restaurá-lo.'
                : 'A página tenta automaticamente primeiro. Se a tela ainda apagar, toque no cadeado uma vez.'
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

    const button = $('wakeBtn');
    if (button) {
        button.addEventListener('click', async () => {
            if (wakeIsActive(wakeState)) {
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

const STATUS_VIEW = {
    live:   { cls: 'tp-pill ok',     label: 'AO VIVO' },
    stale:  { cls: 'tp-pill warn',   label: 'DESATUALIZADO' },
    nodata: { cls: 'tp-pill danger', label: 'SEM DADOS' },
};

const setStatus = (kind) => {
    const pill = $('statusPill');
    if (!pill) return;
    const view = STATUS_VIEW[kind] || STATUS_VIEW.nodata;
    pill.className = view.cls;
    setText('statusLabel', view.label);
};

// Cells in series. Fixed while Settings has no cellCount field; the drawer
// says whether the per-cell figure was measured or derived from this.
const PACK_CELLS = 14;

const SIGNAL_CHIP_TEXT = { s: 'DESATUALIZADO', i: 'INVÁLIDO', a: 'SEM DADO' };
const SIGNAL_SRC_TEXT = { can: 'CAN', ntc: 'NTC' };

// ============ Gauges ============
// 270 deg sweep, so the usable track is three quarters of the circumference.
const R_BATT = 80;
const R_TEMP = 46;
const SWEEP = 0.75;

const circumference = (r) => 2 * Math.PI * r;
const arcLength = (r, frac) => circumference(r) * SWEEP * Math.max(0, Math.min(1, frac));

const setArc = (id, r, frac) => {
    const el = $(id);
    if (!el) return;
    el.setAttribute('stroke-dasharray', `${arcLength(r, frac).toFixed(1)} ${circumference(r).toFixed(2)}`);
};

// A band running from `from` (0..1) to the end of the track.
const setZone = (id, r, from) => {
    const el = $(id);
    if (!el) return;
    el.setAttribute('stroke-dasharray', `${arcLength(r, 1 - from).toFixed(1)} ${circumference(r).toFixed(2)}`);
    el.setAttribute('stroke-dashoffset', `-${arcLength(r, from).toFixed(1)}`);
};

// Full scale and reduction band come from Settings, not from constants here:
// a gauge drawn against the wrong limit is worse than no gauge. These are the
// shipped defaults, used only until /api/config/thermal answers.
let thermal = { motorMax: 100, motorRed: 80, escMax: 110, escRed: 80 };

const applyThermalZones = () => {
    setZone('motorZone', R_TEMP, thermal.motorRed / thermal.motorMax);
    setZone('escZone', R_TEMP, thermal.escRed / thermal.escMax);
};

// Thresholds only change when the pilot edits settings, so this is fetched
// once instead of riding the 1 Hz telemetry payload.
const loadThermalScale = () =>
    fetchJson('/api/config/thermal')
        .then((d) => {
            const toC = (mc) => (mc || 0) / 1000;
            if (d.motorMaxTemp) thermal.motorMax = toC(d.motorMaxTemp);
            if (d.motorTempReductionStart) thermal.motorRed = toC(d.motorTempReductionStart);
            if (d.escMaxTemp) thermal.escMax = toC(d.escMaxTemp);
            if (d.escTempReductionStart) thermal.escRed = toC(d.escTempReductionStart);
        })
        .catch(() => { /* keep the defaults */ })
        .then(applyThermalZones);

// Renders one thermal instrument. A signal that is not valid draws no arc and
// shows an em dash plus a chip saying why -- never a fabricated number for a
// reading we do not trust.
const renderInstrument = (tileId, valueId, arcId, code, celsius, fullScale) => {
    const tile = $(tileId);
    const valueEl = $(valueId);
    if (!tile || !valueEl) return;

    const valid = !code || code === 'v';
    valueEl.textContent = valid ? celsius.toFixed(0) : '—';
    tile.classList.toggle('faulted', !valid);
    if (valid) setArc(arcId, R_TEMP, fullScale > 0 ? celsius / fullScale : 0);

    let chip = tile.querySelector('.tp-chip.signal');
    if (valid) {
        if (chip) chip.remove();
        return;
    }
    if (!chip) {
        chip = document.createElement('span');
        chip.className = 'tp-chip signal';
        tile.appendChild(chip);
    }
    chip.textContent = SIGNAL_CHIP_TEXT[code] || code;
};

// ============ Voltage cell ============
// Pack total by default, per-cell on tap, remembered across visits.
//
// The unit switches with the mode on purpose. Two very different numbers would
// otherwise share the label "Tensão": 58.40 and 4.16 are both plausible-looking
// voltages, and a pilot used to reading the pack total could take a per-cell
// figure for a flat battery. The unit is what makes the number unambiguous.
//
// Per-cell carries a second ambiguity of its own: the BMS minimum cell is a
// measurement, while pack/14 is a mean dressed up as a minimum. On an
// unbalanced pack the mean reads healthy while the worst cell is already low,
// and the BMS is BLE, so the source can change mid-flight with nothing else on
// screen moving. The tilde marks the calculated one; the drawer names both.
const VOLTAGE_MODE_KEY = 'tpVoltMode';
let voltageMode = storeGet('localStorage', VOLTAGE_MODE_KEY) === 'cell' ? 'cell' : 'pack';
let lastTelemetry = null;

const renderVoltage = (data) => {
    if (!data) return;
    const signals = data.signals || {};
    const valid = !signals.battV || signals.battV === 'v';
    const packMv = data.batteryVoltageMv || 0;
    const perCell = voltageMode === 'cell';

    setText('voltageUnit', perCell ? ' V/cél' : ' V');

    if (!valid) {
        setText('voltageValue', '—');
        return;
    }

    if (!perCell) {
        setText('voltageValue', (packMv / 1000).toFixed(2));
        return;
    }

    const bmsCells = !!(data.bms && data.bms.available && data.bms.cellMinMv != null);
    const cellMv = bmsCells ? data.bms.cellMinMv : (packMv / PACK_CELLS);
    setText('voltageValue', `${bmsCells ? '' : '~'}${(cellMv / 1000).toFixed(2)}`);
};

const initVoltageToggle = () => {
    const cell = $('voltageCell');
    if (!cell) return;
    cell.addEventListener('click', () => {
        voltageMode = voltageMode === 'cell' ? 'pack' : 'cell';
        // storeSet swallows a blocked storage, so the toggle still works for
        // this session even where the preference cannot be persisted.
        storeSet('localStorage', VOLTAGE_MODE_KEY, voltageMode);
        renderVoltage(lastTelemetry);
    });
};

const renderTelemetry = (data) => {
    const av = data.availability || {};
    const signals = data.signals || {};

    if (!data.hasTelemetry) {
        setStatus('nodata');
    } else {
        const age = data.uptimeMs - data.lastTelemetryUpdateMs;
        setStatus(age > 3000 ? 'stale' : 'live');
    }

    const armed = !!data.armed;
    const armedPill = $('armedPill');
    if (armedPill) armedPill.className = armed ? 'tp-pill danger' : 'tp-pill off';
    setText('armedLabel', armed ? 'ARMADO' : 'DESARMADO');
    setText('sessionTime', fmtSeconds(data.sessionSec || 0));
    setText('drawerSession', fmtSeconds(data.sessionSec || 0));

    // --- battery ---------------------------------------------------------
    const soc = data.batteryPercentCc ?? 0;
    setText('soc', `${soc}`);
    setArc('battArc', R_BATT, soc / 100);

    renderVoltage(data);
    const bmsCells = !!(data.bms && data.bms.available && data.bms.cellMinMv != null);
    const battValid = !signals.battV || signals.battV === 'v';
    setText('cellSource', bmsCells ? 'BMS · menor célula' : `Calculado ÷ ${PACK_CELLS}S`);
    setText('packVoltage', battValid ? fmtV(data.batteryVoltageMv || 0) : '—');

    // No current on this build (or not yet): drop the cell, centre what is
    // left. Never an empty box, never an "N/A".
    const hasCurrent = !!av.current;
    const cellCurrent = $('cellCurrent');
    if (cellCurrent) cellCurrent.style.display = hasCurrent ? '' : 'none';
    const heroCells = $('heroCells');
    if (heroCells) heroCells.classList.toggle('single', !hasCurrent);
    if (hasCurrent) setText('packCurrent', ((data.escCurrentMa ?? 0) / 1000).toFixed(0));

    // --- instruments -----------------------------------------------------
    const hasPower = !!av.powerKw;
    const tilePower = $('tilePower');
    if (tilePower) tilePower.style.display = hasPower ? '' : 'none';
    const instruments = $('instruments');
    if (instruments) instruments.classList.toggle('cols-2', !hasPower);
    if (hasPower) setText('powerKw', ((data.powerKwX10 ?? 0) / 10).toFixed(1));

    renderInstrument('tileMotor', 'motorTemp', 'motorArc', signals.motorTemp,
                     (data.motorTempMc || 0) / 1000, thermal.motorMax);
    setText('motorSensor', SIGNAL_SRC_TEXT[signals.motorTempSrc] || '');
    renderInstrument('tileEsc', 'escTemp', 'escArc', signals.escTemp,
                     (data.escTempMc || 0) / 1000, thermal.escMax);

    // --- throttle --------------------------------------------------------
    const thr = data.throttlePercent || 0;
    setText('throttlePercent', `${thr}`);
    const fill = $('throttleFill');
    if (fill) fill.style.width = `${thr}%`;

    // --- drawer ----------------------------------------------------------
    setText('rpm', av.rpm ? `${data.rpm ?? 0} rpm` : 'N/A');
    setText('escCurrent', av.current ? fmtA(data.escCurrentMa ?? 0) : 'N/A');
    setText('motorSrcRow', SIGNAL_SRC_TEXT[signals.motorTempSrc] || 'N/A');
    setText('socVoltage', `${data.batteryPercentVoltage || 0} %`);
    setText('throttleRaw', `${data.throttleRaw || 0}`);
    setText('hourMeter', fmtSeconds(data.hourMeterSec || 0));
    renderBmsRows(data);

    bzProcessEvents(data.buzzer);
    renderPowerAlert(data.powerAlert);
    renderFaultDisarm(data);
};

const BMS_STATE_TEXT = {
    connecting: 'Conectando...',
    connected: 'Conectado, aguardando dados...',
};

const renderBmsRows = (data) => {
    const configured = !!data.bmsConfigured;
    const hasData = !!(data.bms && data.bms.available);
    const rowBms = $('rowBms');
    if (rowBms) rowBms.style.display = (configured || hasData) ? '' : 'none';

    const detailRows = ['rowCells', 'rowDelta', 'rowBmsTemp'];
    detailRows.forEach((id) => {
        const el = $(id);
        if (el) el.style.display = hasData ? '' : 'none';
    });

    if (!configured && !hasData) return;

    if (!hasData) {
        const state = data.bmsState || 'none';
        setText('bmsStatus', BMS_STATE_TEXT[state] || `Configurado (${state})`);
        return;
    }

    setText('bmsStatus', 'Conectado');
    const bms = data.bms;
    setText('bmsTempMax', bms.tempMaxC != null ? `${bms.tempMaxC} °C` : '--');
    setText('bmsDelta', bms.cellDeltaMv != null ? `${bms.cellDeltaMv} mV` : '--');
    setText('bmsCells', (bms.cellMinMv != null && bms.cellMaxMv != null)
        ? `${bms.cellMinMv} – ${bms.cellMaxMv} mV`
        : '--');
};

// ============ Power limiting ============
// No banner: the causing tile turns red and the power readout says how much
// is left. A banner would have to appear and disappear, moving every number
// on the panel at the exact moment the pilot is reading them.

const PA_CAUSE_TILES = {
    battery:   'tileBattery',
    motorTemp: 'tileMotor',
    escTemp:   'tileEsc',
};

// Last powerPercent seen, used for the ceiling chip and the throttle-bar tick.
let paPowerPercent = 100;

const renderPowerAlert = (pa) => {
    const causes = (pa && pa.causes) ? pa.causes : [];

    Object.keys(PA_CAUSE_TILES).forEach((key) => {
        const el = $(PA_CAUSE_TILES[key]);
        if (el) el.classList.toggle('limiting', causes.includes(key));
    });

    const limiting = causes.length > 0;
    const tilePower = $('tilePower');
    if (tilePower) tilePower.classList.toggle('limiting', limiting);

    const avail = $('powerAvail');
    const pct = paPowerPercent;
    if (avail) {
        avail.style.display = limiting ? '' : 'none';
        if (limiting) avail.textContent = `DISPONÍVEL ${pct}%`;
    }

    const cap = $('throttleCap');
    if (cap) {
        cap.style.display = limiting ? 'block' : 'none';
        if (limiting) cap.style.left = `${pct}%`;
    }
};

// ============ Fault disarm ============
const FAULT_DISARM_INFO = {
    'THR ERR':  { title: 'Desarmado: falha no acelerador (com fio)', detail: 'Leitura fora da faixa calibrada ou falha de leitura do ADS1115.' },
    'LINK ERR': { title: 'Desarmado: falha no acelerador (sem fio)', detail: 'Link com o remote perdido por mais de 3 segundos.' },
    'MOT ERR':  { title: 'Desarmado: falha no sensor de temperatura do motor', detail: 'Estava válido ao armar e tornou-se inválido depois do armamento.' },
    'MOT SRC':  { title: 'Desarmado: o sensor da temperatura do motor mudou (CAN ⇄ NTC) durante o voo', detail: 'Os limites de temperatura são calibrados por sensor. Verifique o conector do NTC e o Status 5 do ESC antes de armar.' },
    'ESC ERR':  { title: 'Desarmado: falha no sensor de temperatura do ESC', detail: 'Estava válido ao armar e tornou-se inválido depois do armamento.' },
    'BATT ERR': { title: 'Desarmado: falha no sensor de tensão da bateria', detail: 'Estava válido ao armar e tornou-se inválido depois do armamento.' },
};

// The chip lives in space the status bar already reserves, so a fault never
// resizes the panel. The explanation goes in the drawer, one tap away.
const renderFaultDisarm = (data) => {
    const chip = $('faultChip');
    const block = $('faultBlock');
    if (!chip || !block) return;

    const reason = data.disarmReason || '';
    const isFault = !data.armed && reason !== '' && reason !== 'MANUAL';

    chip.style.display = isFault ? '' : 'none';
    block.style.display = isFault ? '' : 'none';

    // A fault only exists while disarmed, so the DESARMADO pill would just be
    // repeating the chip -- and its width is what the chip needs.
    const armedPill = $('armedPill');
    if (armedPill) armedPill.style.display = isFault ? 'none' : '';

    if (!isFault) return;

    const info = FAULT_DISARM_INFO[reason];
    setText('faultCode', reason);
    setText('faultTitle', info ? info.title : 'Desarmado por falha');
    setText('faultDetail', info ? info.detail : `Código: ${reason}`);
};

// ============ Drawer ============
const setDrawer = (open) => {
    const drawer = $('drawer');
    const scrim = $('scrim');
    if (drawer) drawer.classList.toggle('open', open);
    if (scrim) scrim.classList.toggle('open', open);
};

const initDrawer = () => {
    const moreBtn = $('moreBtn');
    if (moreBtn) moreBtn.addEventListener('click', () => setDrawer(true));

    const closeBtn = $('drawerClose');
    if (closeBtn) closeBtn.addEventListener('click', () => setDrawer(false));

    const scrim = $('scrim');
    if (scrim) scrim.addEventListener('click', () => setDrawer(false));

    const chip = $('faultChip');
    if (chip) chip.addEventListener('click', () => setDrawer(true));
};

const initSessionReset = () => {
    const btn = $('resetSessionButton');
    if (!btn) return;
    btn.addEventListener('click', () => {
        if (!confirm('Tem certeza que deseja resetar o tempo de voo?')) return;
        // This page is self-contained (no COMMON_JS, no PIN field), so on a
        // fresh load cfgPin may be empty. On 403 we prompt for the PIN, cache
        // it (same key the config pages use) and retry once. The server replies
        // 403 text/plain for a bad PIN, so we must not assume JSON here.
        const headers = { 'X-Config-Pin': storeGet('sessionStorage', 'cfgPin') || '' };
        fetch('/api/session/reset', { method: 'POST', headers })
            .then((r) => {
                if (r.status !== 403) return r;
                const pin = prompt('PIN de configuração:');
                if (!pin) return null;
                storeSet('sessionStorage', 'cfgPin', pin);
                return fetch('/api/session/reset', {
                    method: 'POST',
                    headers: { 'X-Config-Pin': pin },
                });
            })
            .then((r) => {
                if (!r) return;
                if (r.status === 403) {
                    alert('PIN inválido');
                } else if (!r.ok) {
                    alert('Falha ao resetar o tempo de voo');
                } else {
                    setText('sessionTime', fmtSeconds(0));
                    setText('drawerSession', fmtSeconds(0));
                }
            })
            .catch(() => alert('Erro ao comunicar com o servidor'));
    });
};

const loadTelemetry = () => {
    fetchJson('/api/telemetry')
        .then((data) => {
            paPowerPercent = data.powerPercent != null ? data.powerPercent : 100;
            lastTelemetry = data;
            renderTelemetry(data);
        })
        .catch(() => setStatus('nodata'));
};

document.addEventListener('DOMContentLoaded', () => {
    initTelemetryWake();
    initBuzzerSound();
    initDrawer();
    initVoltageToggle();
    initSessionReset();
    loadThermalScale();
    loadTelemetry();
    setInterval(loadTelemetry, 1000);
});
)rawliteral";
