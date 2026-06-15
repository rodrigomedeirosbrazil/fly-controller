# Browser Buzzer Beeps Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Espelhar os beeps do buzzer físico do controller na página de telemetria do web portal, com fidelidade de pitch, ritmo e repetições.

**Architecture:** `Buzzer` expõe um snapshot `BeepEvent` (seq, freq, onMs, offMs, reps, active) preenchido em `startBeep()`/`stop()`. O `/api/telemetry` inclui esse snapshot no JSON existente (piggyback no poll de 1s). O JS da página de telemetria compara o `seq` recebido com o último visto, toca o padrão via Web Audio API (OscillatorNode square → GainNode) e mantém um loop local para padrões contínuos. Um botão 🔔/🔇 na barra de status destrava o `AudioContext` e serve de mute/unmute.

**Tech Stack:** C++ Arduino/PlatformIO (ESP32-C3), ArduinoJSON, Web Audio API (OscillatorNode, GainNode, AudioContext), localStorage.

---

## File Map

| Arquivo | Ação | Responsabilidade |
|---|---|---|
| `controller/src/Buzzer/Buzzer.h` | Modificar | Adicionar struct `BeepEvent`, membro privado `beepEvent_`, declaração de `getBeepEvent()` |
| `controller/src/Buzzer/Buzzer.cpp` | Modificar | Preencher `beepEvent_` em `startBeep()` e `stop()`; implementar `getBeepEvent()` |
| `controller/src/WebServer/ControllerWebServer.cpp` | Modificar | Adicionar objeto `buzzer` ao JSON de `/api/telemetry`; aumentar `StaticJsonDocument` |
| `controller/src/WebServer/Pages/TelemetryPage.h` | Modificar | Botão 🔔/🔇 no HTML; Web Audio + state machine no JS inline |

---

## Task 1: Adicionar BeepEvent ao Buzzer.h

**Files:**
- Modify: `controller/src/Buzzer/Buzzer.h`

- [ ] **Step 1: Adicionar struct BeepEvent e membro privado ao Buzzer.h**

Abrir `controller/src/Buzzer/Buzzer.h`. Adicionar após o `struct Melody` (linha 17) e adicionar membro privado à classe:

```cpp
// Snapshot of the currently playing (or last played) beep, for web telemetry.
struct BeepEvent {
    uint32_t seq;       // monotonic counter, incremented on each startBeep()
    uint16_t frequency; // Hz (resolved default if 0 was passed)
    uint16_t onMs;      // on duration
    uint16_t offMs;     // off/pause duration between reps
    uint8_t  reps;      // 255 = continuous
    bool     active;    // true while the buzzer is playing
};
```

Na seção `private:` da classe, adicionar após `bool melodyRepeat;`:

```cpp
    BeepEvent beepEvent_;
```

Na seção `public:`, adicionar após `void stop();`:

```cpp
    BeepEvent getBeepEvent() const { return beepEvent_; }
```

- [ ] **Step 2: Compilar para verificar que o header está OK**

```bash
cd controller && ~/.platformio/penv/bin/pio run -e lolin_c3_mini_hobbywing 2>&1 | tail -5
```

Esperado: `SUCCESS` (sem erros novos).

- [ ] **Step 3: Commit**

```bash
git add controller/src/Buzzer/Buzzer.h
git commit -m "feat(buzzer): add BeepEvent struct and getBeepEvent() declaration"
```

---

## Task 2: Implementar BeepEvent tracking no Buzzer.cpp

**Files:**
- Modify: `controller/src/Buzzer/Buzzer.cpp`

- [ ] **Step 1: Inicializar beepEvent_ no construtor**

No construtor `Buzzer::Buzzer(...)`, ao final da lista de inicializadores (após `melodyRepeat(false)`), adicionar:

```cpp
  beepEvent_{}
```

(Inicializa todos os campos para zero/false via aggregate initialization.)

O construtor completo ficará:

```cpp
Buzzer::Buzzer(uint8_t buzzerPin) :
  pin(buzzerPin),
  pwmChannel(1),
  pwmFrequency(kDefaultBeepFrequencyHz),
  pwmResolution(8),
  pwmDutyCycle(kDefaultDutyCycle),
  playing(false),
  startTime(0),
  beepDuration(0),
  pauseDuration(0),
  repetitions(0),
  currentRepetition(0),
  isOn(false),
  playingMelody(false),
  currentMelody(nullptr),
  currentNoteIndex(0),
  noteStartTime(0),
  noteIsOn(false),
  melodyRepeat(false),
  beepEvent_{} {
}
```

- [ ] **Step 2: Preencher beepEvent_ em startBeep()**

Em `startBeep()`, após a chamada `startTime = millis();` (última linha do método), adicionar:

```cpp
  beepEvent_.seq++;
  beepEvent_.frequency = pwmFrequency;  // resolved: setFrequency() already updated it
  beepEvent_.onMs      = duration;
  beepEvent_.offMs     = pause;
  beepEvent_.reps      = reps;
  beepEvent_.active    = true;
```

Nota: `setFrequency(frequency)` é chamada antes se `frequency > 0`, então `pwmFrequency` já contém o valor resolvido na hora de preencher o snapshot.

- [ ] **Step 3: Setar active=false em stop()**

Em `stop()`, após `setPwmOff();` (última linha), adicionar:

```cpp
  beepEvent_.active = false;
```

- [ ] **Step 4: Compilar para verificar**

```bash
cd controller && ~/.platformio/penv/bin/pio run -e lolin_c3_mini_hobbywing 2>&1 | tail -5
```

Esperado: `SUCCESS`.

- [ ] **Step 5: Verificar os outros targets**

```bash
~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor 2>&1 | tail -3
~/.platformio/penv/bin/pio run -e lolin_c3_mini_xag 2>&1 | tail -3
```

Esperado: `SUCCESS` nos três.

- [ ] **Step 6: Commit**

```bash
git add controller/src/Buzzer/Buzzer.cpp
git commit -m "feat(buzzer): track BeepEvent snapshot in startBeep() and stop()"
```

---

## Task 3: Adicionar campo buzzer ao /api/telemetry

**Files:**
- Modify: `controller/src/WebServer/ControllerWebServer.cpp` (linha 652–726)

- [ ] **Step 1: Aumentar o StaticJsonDocument e adicionar bloco buzzer**

Localizar `StaticJsonDocument<768> doc;` na linha ~653. Mudar para `896` para acomodar os ~120 bytes do objeto buzzer:

```cpp
StaticJsonDocument<896> doc;
```

Após o bloco `if (isBmsDataAvailable()) { ... }` (antes do `if (doc.overflowed())`), adicionar:

```cpp
        {
            const BeepEvent ev = buzzer.getBeepEvent();
            JsonObject buzzerObj = doc.createNestedObject("buzzer");
            buzzerObj["seq"]    = ev.seq;
            buzzerObj["freq"]   = ev.frequency;
            buzzerObj["onMs"]   = ev.onMs;
            buzzerObj["offMs"]  = ev.offMs;
            buzzerObj["reps"]   = ev.reps;
            buzzerObj["active"] = ev.active;
        }
```

- [ ] **Step 2: Compilar para verificar**

```bash
cd controller && ~/.platformio/penv/bin/pio run -e lolin_c3_mini_hobbywing 2>&1 | tail -5
```

Esperado: `SUCCESS`.

- [ ] **Step 3: Verificar o JSON via telemetria (opcional, se houver hardware)**

Conectar ao AP `FlyController`, abrir `http://192.168.4.1/api/telemetry`. O JSON deve conter:
```json
"buzzer":{"seq":0,"freq":2300,"onMs":0,"offMs":0,"reps":0,"active":false}
```

- [ ] **Step 4: Commit**

```bash
git add controller/src/WebServer/ControllerWebServer.cpp
git commit -m "feat(webserver): include BeepEvent snapshot in /api/telemetry response"
```

---

## Task 4: Botão de som + Web Audio na TelemetryPage.h

**Files:**
- Modify: `controller/src/WebServer/Pages/TelemetryPage.h`

Esta task tem dois sub-passos: HTML e JS.

### 4a — HTML: botão 🔔/🔇

- [ ] **Step 1: Adicionar botão na barra de status do HTML**

No `TELEMETRY_PAGE_HTML`, localizar a `div.tsb-right` que contém `wakeIconBtn`. Adicionar o botão de som **antes** do `wakeIconBtn`:

```html
                <div class="tsb-right">
                    <button type="button" id="soundBtn" class="wake-icon-btn sound-muted" aria-label="Ativar som">&#x1F507;</button>
                    <button type="button" id="wakeIconBtn" class="wake-icon-btn" aria-expanded="false" aria-controls="wakePanel">&#x1F512;</button>
                </div>
```

O ícone `🔇` é `&#x1F507;`, `🔔` é `&#x1F514;`.

### 4b — JS: Web Audio + state machine

- [ ] **Step 2: Adicionar Web Audio e state machine ao TELEMETRY_PAGE_JS**

Após as constantes de formatação (`fmtC`, `fmtV`, etc.) e **antes** da seção de wake-lock (linha com `let wakeLockSentinel = null;`), inserir o bloco inteiro de Web Audio:

```javascript
// ============ Web Audio Buzzer ============
let audioCtx = null;
let gainMaster = null;
let soundMuted = localStorage.getItem('bzMuted') !== '0';  // default: muted
let bzLoopActive = false;
let bzStopLoopFlag = false;
let bzActiveOsc = null;
let bzActiveGain = null;
let bzLastSeq = -1;
let bzLoopIsRunning = false;

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
    if (soundMuted) {
        btn.innerHTML = '&#x1F507;';
        btn.classList.add('sound-muted');
        btn.setAttribute('aria-label', 'Ativar som');
    } else {
        btn.innerHTML = '&#x1F514;';
        btn.classList.remove('sound-muted');
        btn.setAttribute('aria-label', 'Silenciar');
    }
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

const bzPlayOnce = (freq, onMs, offMs, reps) => {
    if (!audioCtx) return;
    bzStopLoop();
    let t = audioCtx.currentTime;
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

// Pure function — easy to test in isolation
const bzNextAction = (prevSeq, curSeq, reps, active) => {
    if (curSeq === prevSeq) return 'none';
    if (!active)            return 'stop';
    if (reps === 255)       return 'start-loop';
    return 'play-once';
};

const bzProcessEvent = (bz) => {
    if (!bz) return;
    const action = bzNextAction(bzLastSeq, bz.seq, bz.reps, bz.active);
    if (action === 'none') return;
    bzLastSeq = bz.seq;
    if (action === 'play-once') {
        bzPlayOnce(bz.freq, bz.onMs, bz.offMs, bz.reps);
    } else if (action === 'start-loop') {
        bzStartLoop(bz.freq, bz.onMs, bz.offMs);
    } else if (action === 'stop') {
        bzStopLoop();
    }
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
        localStorage.setItem('bzMuted', soundMuted ? '1' : '0');
        bzApplyMute();
    });
};
// ============ End Web Audio Buzzer ============
```

- [ ] **Step 3: Chamar bzProcessEvent() no renderTelemetry() e initBuzzerSound() no DOMContentLoaded**

Em `renderTelemetry(data)`, **ao final da função** (antes do `};` de fechamento), adicionar:

```javascript
    bzProcessEvent(data.buzzer);
```

Em `document.addEventListener('DOMContentLoaded', ...)`, adicionar `initBuzzerSound();` após `initTelemetryWake();`:

```javascript
document.addEventListener('DOMContentLoaded', () => {
    initTelemetryWake();
    initBuzzerSound();
    loadTelemetry();
    setInterval(loadTelemetry, 1000);
});
```

- [ ] **Step 4: Compilar os três targets**

```bash
cd controller
~/.platformio/penv/bin/pio run -e lolin_c3_mini_hobbywing 2>&1 | tail -5
~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor 2>&1 | tail -3
~/.platformio/penv/bin/pio run -e lolin_c3_mini_xag 2>&1 | tail -3
```

Esperado: `SUCCESS` nos três.

- [ ] **Step 5: Commit**

```bash
git add controller/src/WebServer/Pages/TelemetryPage.h
git commit -m "feat(telemetry): add browser buzzer beeps via Web Audio API"
```

---

## Task 5: Verificação final

- [ ] **Step 1: Build completo dos quatro targets**

```bash
cd controller
~/.platformio/penv/bin/pio run -e lolin_c3_mini_hobbywing 2>&1 | tail -5
~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor 2>&1 | tail -3
~/.platformio/penv/bin/pio run -e lolin_c3_mini_xag 2>&1 | tail -3
cd ../throttle && ~/.platformio/penv/bin/pio run -e remote_throttle 2>&1 | tail -3
```

Esperado: `SUCCESS` nos quatro.

- [ ] **Step 2: Teste manual do JS (bzNextAction) no console do browser**

Abrir `controller/src/WebServer/Pages/BuzzerPlayground.html` (ou qualquer página) e no console do browser executar:

```javascript
const bzNextAction = (prevSeq, curSeq, reps, active) => {
    if (curSeq === prevSeq) return 'none';
    if (!active)            return 'stop';
    if (reps === 255)       return 'start-loop';
    return 'play-once';
};

console.assert(bzNextAction(0, 0, 3, true)    === 'none',       'seq same -> none');
console.assert(bzNextAction(0, 1, 3, true)    === 'play-once',  'one-shot');
console.assert(bzNextAction(0, 1, 255, true)  === 'start-loop', 'continuous');
console.assert(bzNextAction(0, 1, 3, false)   === 'stop',       'active=false -> stop');
console.assert(bzNextAction(5, 5, 255, false) === 'none',       'same seq inactive -> none');
console.log('bzNextAction: all tests passed');
```

Esperado: `bzNextAction: all tests passed` sem assertion errors.

- [ ] **Step 3: Teste funcional no hardware (ou via playground)**

Usando o `BuzzerPlayground.html` (já criado em `controller/src/WebServer/Pages/`):
1. Abrir em `http://localhost:8765/BuzzerPlayground.html`
2. Clicar em 🔇 para desmutir
3. Testar cada cenário e confirmar que pitch e ritmo batem com o firmware

Ou, se houver hardware disponível:
1. Flash no controller, conectar ao AP `FlyController`
2. Abrir `http://192.168.4.1/telemetry`
3. Clicar em 🔇, armar o throttle → alerta contínuo deve tocar no browser
4. Desarmar → browser para de tocar

- [ ] **Step 4: Commit de fechamento (se necessário)**

Se houver ajustes menores no Step 3:

```bash
git add -p  # staged apenas o que foi ajustado
git commit -m "fix(telemetry): adjust browser beep timing from hardware test"
```
