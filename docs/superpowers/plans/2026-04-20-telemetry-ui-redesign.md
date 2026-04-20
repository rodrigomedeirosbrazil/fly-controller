# Telemetry UI Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Redesign the telemetry page to fit all 7 data cards on screen without scrolling in portrait on iPhone/Android, with larger readable sub-text and safety-colored armed state.

**Architecture:** All changes are confined to two files: `CommonStyles.h` (CSS) and `TelemetryPage.h` (HTML + JS). No new files are created. CSS changes are applied first so each HTML/JS task produces a visually consistent result.

**Tech Stack:** Embedded HTML/CSS/JS in C++ header string literals (PROGMEM). Build tool: PlatformIO (`~/.platformio/penv/bin/pio`).

**Spec:** `docs/superpowers/specs/2026-04-20-telemetry-ui-redesign.md`

---

### Task 1: CSS — Remove obsolete hero-card rules

**Files:**
- Modify: `src/WebServer/Pages/CommonStyles.h`

Remove the block of rules that apply only to the hero cards that will be replaced. These are safe to delete because the hero grid HTML will be replaced in Task 6.

- [ ] **Step 1: Remove the following CSS blocks from `CommonStyles.h`**

Find and delete these rules (they appear between the `.status` rules and the `@media (max-width: 600px)` block):

```css
.telemetry-header-copy {
    min-width: 0;
}

.telemetry-hero-grid {
    display: grid;
    grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: 10px;
}

.hero-card {
    display: flex;
    flex-direction: column;
    min-height: 112px;
}

.hero-main {
    margin-top: 10px;
    display: flex;
    align-items: center;
    gap: 6px;
    flex-wrap: wrap;
}

.hero-actions {
    margin-top: 10px;
    display: flex;
    align-items: center;
    gap: 6px;
}

.telemetry-header h2,
.wake-card h2 {
    margin: 0;
    font-size: 13px;
    letter-spacing: 0.06em;
    text-transform: uppercase;
    color: var(--muted);
}

.wake-button {
    flex-shrink: 0;
    min-height: 34px;
    min-width: 0;
    padding: 0 12px;
    border-radius: 10px;
    font-size: 13px;
}

.help-button {
    width: 28px;
    height: 28px;
    border: 1px solid var(--border);
    border-radius: 999px;
    background: #f8fafc;
    color: var(--muted);
    font-size: 16px;
    font-weight: 700;
    line-height: 1;
    padding: 0;
}

.telemetry-header .status,
.wake-card .status {
    min-height: 28px;
    padding: 0 10px;
    font-size: 11px;
}

.help-panel {
    display: none;
    margin-top: 12px;
    color: var(--muted);
    font-size: 14px;
    line-height: 1.35;
}

.help-panel.open {
    display: block;
}
```

Also remove from the `@media (max-width: 768px)` block:

```css
    .telemetry-hero-grid {
        gap: 8px;
    }

    .telemetry-header.panel {
        flex-shrink: 0;
        margin-bottom: 0;
        padding: 8px 10px;
    }

    .wake-card.panel {
        flex-shrink: 0;
        margin-bottom: 0;
        padding: 8px 10px;
    }

    .telemetry-header.panel h1 {
        font-size: 18px;
        margin-bottom: 0;
        line-height: 1.05;
    }

    .hero-card {
        min-height: 96px;
    }

    .hero-main {
        margin-top: 8px;
        gap: 5px;
    }

    .hero-actions {
        margin-top: 8px;
        gap: 5px;
    }

    .telemetry-header .status,
    .wake-card .status {
        min-height: 24px;
        padding: 0 8px;
        font-size: 10px;
    }

    .wake-button {
        min-height: 30px;
        padding: 0 10px;
        font-size: 12px;
    }

    .help-button {
        width: 24px;
        height: 24px;
        font-size: 14px;
    }

    .help-panel {
        margin-top: 8px;
        font-size: 12px;
        line-height: 1.25;
    }
```

- [ ] **Step 2: Commit**

```bash
cd /Users/rodrigo/dev/fly-controller
git add src/WebServer/Pages/CommonStyles.h
git commit -m "style: remove obsolete telemetry hero-card CSS rules"
```

---

### Task 2: CSS — Add status bar, wake panel, armed pill, sub2 pattern

**Files:**
- Modify: `src/WebServer/Pages/CommonStyles.h`

Add all new CSS rules after the `.telemetry-grid .sub.bms-meta` block (before the `@media (max-width: 600px)` rule).

- [ ] **Step 1: Add new rules to `CommonStyles.h`**

Insert the following block after the existing `.telemetry-grid .value.bms-value` rule:

```css
.telemetry-status-bar {
    display: flex;
    align-items: center;
    background: var(--card);
    border-radius: var(--radius);
    padding: 0 12px;
    height: 38px;
    flex-shrink: 0;
    box-shadow: var(--shadow);
    gap: 8px;
    margin-bottom: 0;
}

.telemetry-status-bar .tsb-label {
    font-size: 10px;
    font-weight: 700;
    color: var(--muted);
    text-transform: uppercase;
    letter-spacing: 0.05em;
    white-space: nowrap;
}

.telemetry-status-bar .tsb-mid {
    display: flex;
    align-items: center;
    gap: 5px;
    flex: 1;
}

.telemetry-status-bar .tsb-right {
    display: flex;
    align-items: center;
    flex-shrink: 0;
}

.wake-icon-btn {
    width: 30px;
    height: 30px;
    border-radius: 8px;
    background: #f3f4f6;
    border: 1px solid var(--border);
    cursor: pointer;
    font-size: 15px;
    line-height: 1;
    padding: 0;
    display: flex;
    align-items: center;
    justify-content: center;
}

.wake-panel {
    display: none;
    background: var(--card);
    border-radius: var(--radius);
    padding: 10px 12px;
    flex-shrink: 0;
    box-shadow: var(--shadow);
    font-size: 13px;
    color: var(--muted);
    line-height: 1.4;
}

.wake-panel.open {
    display: block;
}

.wake-panel-row {
    display: flex;
    align-items: center;
    gap: 8px;
    margin-top: 8px;
}

.armed-pill {
    display: inline-flex;
    align-items: center;
    gap: 4px;
    margin-top: 5px;
    padding: 3px 9px;
    border-radius: 999px;
    font-size: 11px;
    font-weight: 700;
    width: fit-content;
    flex-shrink: 0;
}

.armed-pill .armed-dot {
    width: 6px;
    height: 6px;
    border-radius: 50%;
    flex-shrink: 0;
}

.armed-pill.armed {
    background: #fee2e2;
    color: #b91c1c;
}

.armed-pill.armed .armed-dot {
    background: #b91c1c;
}

.armed-pill.disarmed {
    background: #f3f4f6;
    color: #6b7280;
}

.armed-pill.disarmed .armed-dot {
    background: #9ca3af;
}

.sub2 {
    margin-top: 4px;
    flex-shrink: 0;
}

.sub2-label {
    font-size: 10px;
    color: var(--muted);
    font-weight: 600;
    text-transform: uppercase;
    letter-spacing: 0.03em;
    line-height: 1.1;
}

.sub2-value {
    font-size: 15px;
    color: var(--muted);
    font-weight: 600;
    line-height: 1.15;
    margin-top: 1px;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
}
```

- [ ] **Step 2: Commit**

```bash
git add src/WebServer/Pages/CommonStyles.h
git commit -m "style: add status bar, wake panel, armed pill and sub2 CSS"
```

---

### Task 3: CSS — Update typography and BMS grid rules

**Files:**
- Modify: `src/WebServer/Pages/CommonStyles.h`

Update font sizes inside the mobile media query blocks and add BMS full-width rule.

- [ ] **Step 1: Update desktop typography block**

Find and update the desktop media query block (around line 120):

```css
/* Telemetry page: larger cards on desktop only (avoid overriding mobile telemetry rules) */
@media (min-width: 769px) and (min-height: 501px) {
    .page-telemetry .telemetry-grid .label {
        font-size: 13px;
    }

    .page-telemetry .telemetry-grid .value {
        font-size: 24px;
    }

    .page-telemetry .telemetry-grid .sub {
        font-size: 17px;
    }

    .page-telemetry .telemetry-grid .sub2-value {
        font-size: 17px;
    }
}
```

- [ ] **Step 2: Update `.telemetry-grid .label`, `.value`, `.sub` inside the `@media (max-width: 768px)` block**

Find the existing rules:
```css
    .telemetry-grid .label {
        font-size: 11px;
        line-height: 1.05;
    }

    .telemetry-grid .value {
        margin-top: 6px;
        font-size: clamp(27px, 4.4vw, 36px);
        line-height: 1.02;
        white-space: nowrap;
        text-overflow: ellipsis;
        overflow: hidden;
    }

    .telemetry-grid .sub {
        margin-top: 6px;
        font-size: clamp(14px, 2.3vw, 19px);
        line-height: 1.12;
        white-space: nowrap;
        text-overflow: ellipsis;
        overflow: hidden;
    }
```

Replace with:
```css
    .telemetry-grid .label {
        font-size: 11px;
        font-weight: 600;
        line-height: 1.05;
    }

    .telemetry-grid .value {
        margin-top: 6px;
        font-size: clamp(22px, 6vw, 34px);
        line-height: 1.02;
        white-space: nowrap;
        text-overflow: ellipsis;
        overflow: hidden;
    }

    .telemetry-grid .sub {
        margin-top: 4px;
        font-size: 17px;
        font-weight: 500;
        line-height: 1.12;
        white-space: nowrap;
        text-overflow: ellipsis;
        overflow: hidden;
    }

    .telemetry-grid .sub + .sub {
        margin-top: 2px;
    }

    .telemetry-grid .sub2-value {
        font-size: 17px;
    }

    .telemetry-grid .card.bms-card {
        grid-column: 1 / -1;
    }
```

- [ ] **Step 3: Update landscape sub sizes inside the nested landscape media query**

Find the existing landscape block inside the mobile media query:
```css
    @media (orientation: landscape) and (min-width: 480px) {
        .telemetry-grid.grid {
            grid-template-columns: repeat(3, minmax(0, 1fr));
        }
    }

    @media (orientation: landscape) and (min-width: 700px) {
        .telemetry-grid.grid {
            grid-template-columns: repeat(4, minmax(0, 1fr));
        }
    }
```

Replace with:
```css
    @media (orientation: landscape) and (min-width: 480px) {
        .telemetry-grid.grid {
            grid-template-columns: repeat(3, minmax(0, 1fr));
        }

        .telemetry-grid .sub {
            font-size: 14px;
        }

        .telemetry-grid .sub2-value {
            font-size: 14px;
        }

        .telemetry-grid .card.bms-card {
            grid-column: span 2;
        }
    }

    @media (orientation: landscape) and (min-width: 700px) {
        .telemetry-grid.grid {
            grid-template-columns: repeat(4, minmax(0, 1fr));
        }
    }
```

- [ ] **Step 4: Update mobile status bar height inside `@media (max-width: 768px)` block**

Add after the existing `.telemetry-shell` rule:
```css
    .telemetry-status-bar {
        height: 34px;
        padding: 0 10px;
    }

    .wake-icon-btn {
        width: 26px;
        height: 26px;
        font-size: 13px;
    }
```

- [ ] **Step 5: Update topbar to single scrollable row inside `@media (max-width: 768px)` block**

Find:
```css
    body.telemetry-page .page.page-telemetry .topbar {
        flex-shrink: 0;
        margin-bottom: 6px;
        padding-top: 4px;
        padding-bottom: 6px;
    }
```

Replace with:
```css
    body.telemetry-page .page.page-telemetry .topbar {
        flex-shrink: 0;
        flex-wrap: nowrap;
        overflow-x: auto;
        -webkit-overflow-scrolling: touch;
        scrollbar-width: none;
        margin-bottom: 6px;
        padding-top: 4px;
        padding-bottom: 6px;
    }

    body.telemetry-page .page.page-telemetry .topbar::-webkit-scrollbar {
        display: none;
    }
```

- [ ] **Step 6: Commit**

```bash
git add src/WebServer/Pages/CommonStyles.h
git commit -m "style: update telemetry typography, BMS grid and topbar scroll"
```

---

### Task 4: HTML — Replace hero grid with status bar

**Files:**
- Modify: `src/WebServer/Pages/TelemetryPage.h`

- [ ] **Step 1: Replace the `.telemetry-hero-grid` block**

Find:
```html
        <div class="telemetry-hero-grid">
                <div class="panel telemetry-header hero-card">
                    <div class="telemetry-header-copy">
                        <h1>Telemetria ao Vivo</h1>
                        <div class="hero-main">Status dos dados: <span id="statusBadge" class="status nodata">SEM DADOS</span></div>
                    </div>
                </div>

                <div class="panel wake-card hero-card">
                    <h2>Manter Tela Ligada</h2>
                    <div class="hero-actions">
                        <span id="wakeStatusBadge" class="status status-secondary">INATIVO</span>
                        <button type="button" id="wakeHelpToggle" class="help-button" aria-expanded="false" aria-controls="wakeHelp">?</button>
                    </div>
                    <div class="hero-actions">
                        <button type="button" id="wakeToggleButton" class="btn btn-sm wake-button">Manter Ativo</button>
                    </div>
                    <div class="help-panel" id="wakeHelp">A página tentará automaticamente primeiro. Se a tela ainda apagar, toque no botão uma vez.</div>
                </div>
            </div>
```

Replace with:
```html
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
```

- [ ] **Step 2: Commit**

```bash
git add src/WebServer/Pages/TelemetryPage.h
git commit -m "feat: replace telemetry hero cards with thin status bar"
```

---

### Task 5: HTML — Update data cards

**Files:**
- Modify: `src/WebServer/Pages/TelemetryPage.h`

- [ ] **Step 1: Update Tensão card — add armed pill**

Find:
```html
                <div class="card">
                    <div class="label">Tensão da Bateria</div>
                    <div class="value" id="batteryVoltage">--</div>
                </div>
```

Replace with:
```html
                <div class="card">
                    <div class="label">Tensão</div>
                    <div class="value" id="batteryVoltage">--</div>
                    <div class="armed-pill disarmed" id="armedPill">
                        <span class="armed-dot"></span>
                        <span id="armedLabel">DESARMADO</span>
                    </div>
                </div>
```

- [ ] **Step 2: Update Bateria card — CC as main value, voltage as sub2**

Find:
```html
                <div class="card">
                    <div class="label">SoC da Bateria</div>
                    <div class="value" id="soc">--</div>
                    <div class="sub" id="socCc">--</div>
                </div>
```

Replace with:
```html
                <div class="card">
                    <div class="label">Bateria</div>
                    <div class="value" id="soc">--</div>
                    <div class="sub2">
                        <div class="sub2-label">Tens&#xE3;o</div>
                        <div class="sub2-value" id="socVoltage">--</div>
                    </div>
                </div>
```

- [ ] **Step 3: Update Energia card — sub2 pattern**

Find:
```html
                <div class="card">
                    <div class="label">Energia</div>
                    <div class="value" id="powerKw">--</div>
                    <div class="sub" id="powerPercent">--</div>
                </div>
```

Replace with:
```html
                <div class="card">
                    <div class="label">Energia</div>
                    <div class="value" id="powerKw">--</div>
                    <div class="sub2">
                        <div class="sub2-label">Limite</div>
                        <div class="sub2-value" id="powerPercent">--</div>
                    </div>
                </div>
```

- [ ] **Step 4: Update Acelerador card — sub2 pattern**

Find:
```html
                <div class="card">
                    <div class="label">Acelerador</div>
                    <div class="value" id="throttlePercent">--</div>
                    <div class="sub" id="throttleRaw">--</div>
                </div>
```

Replace with:
```html
                <div class="card">
                    <div class="label">Acelerador</div>
                    <div class="value" id="throttlePercent">--</div>
                    <div class="sub2">
                        <div class="sub2-label">Bruto</div>
                        <div class="sub2-value" id="throttleRaw">--</div>
                    </div>
                </div>
```

- [ ] **Step 5: Remove Sistema card**

Find and delete entirely:
```html
                <div class="card">
                    <div class="label">Sistema</div>
                    <div class="value" id="armed">--</div>
                    <div class="sub" id="freshness">--</div>
                </div>
```

- [ ] **Step 6: Update BMS card — two sub lines**

Find:
```html
                <div class="card bms-card" id="bmsCard" style="display: none;">
                    <div class="label">BMS</div>
                    <div class="value bms-value" id="bmsTempMax">--</div>
                    <div class="sub multiline bms-meta" id="bmsCells">--</div>
                </div>
```

Replace with:
```html
                <div class="card bms-card" id="bmsCard" style="display: none;">
                    <div class="label">BMS</div>
                    <div class="value" id="bmsTempMax">--</div>
                    <div class="sub" id="bmsDelta">--</div>
                    <div class="sub" id="bmsCells">--</div>
                </div>
```

- [ ] **Step 7: Commit**

```bash
git add src/WebServer/Pages/TelemetryPage.h
git commit -m "feat: update telemetry data cards layout and content"
```

---

### Task 6: JS — Update renderTelemetry

**Files:**
- Modify: `src/WebServer/Pages/TelemetryPage.h` (the `TELEMETRY_PAGE_JS` section)

- [ ] **Step 1: Update battery SoC rendering — swap CC and voltage**

Find:
```js
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
```

Replace with:
```js
    setText('batteryVoltage', fmtV(data.batteryVoltageMv || 0));
    setText('soc', `${data.batteryPercentCc ?? 0} %`);
    setText('socVoltage', `${data.batteryPercentVoltage || 0} %`);
    setText('powerKw', av.powerKw ? fmtKw(data.powerKwX10 ?? 0) : 'N/A');
    setText('powerPercent', `${data.powerPercent || 0} %`);
    setText('throttlePercent', `${data.throttlePercent || 0} %`);
    setText('throttleRaw', `${data.throttleRaw || 0}`);
    setText('motorTemp', fmtC(data.motorTempMc || 0));
    setText('rpm', av.rpm ? `${data.rpm ?? 0} rpm` : 'N/A');
    setText('escTemp', fmtC(data.escTempMc || 0));
    setText('escCurrent', av.current ? fmtA(data.escCurrentMa ?? 0) : 'N/A');

    const armedPill = $('armedPill');
    if (armedPill) {
        armedPill.className = `armed-pill ${data.armed ? 'armed' : 'disarmed'}`;
    }
    setText('armedLabel', data.armed ? 'ARMADO' : 'DESARMADO');
```

- [ ] **Step 2: Remove freshness update from the hasTelemetry block**

Find:
```js
    if (!data.hasTelemetry) {
        setStatus('nodata');
        setText('freshness', 'Aguardando telemetria');
    } else {
        const age = data.uptimeMs - data.lastTelemetryUpdateMs;
        setStatus(age > 3000 ? 'stale' : 'live');
        setText('freshness', `Última atualização há ${Math.max(0, age)} ms`);
    }
```

Replace with:
```js
    if (!data.hasTelemetry) {
        setStatus('nodata');
    } else {
        const age = data.uptimeMs - data.lastTelemetryUpdateMs;
        setStatus(age > 3000 ? 'stale' : 'live');
    }
```

- [ ] **Step 3: Update BMS rendering — split into bmsDelta and bmsCells**

Find:
```js
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
```

Replace with:
```js
    const bmsCard = $('bmsCard');
    if (data.bms && data.bms.available) {
        bmsCard.style.display = '';
        setText('bmsTempMax', data.bms.tempMaxC != null ? `${data.bms.tempMaxC} C` : '--');
        setText('bmsDelta', data.bms.cellDeltaMv != null ? `Delta: ${data.bms.cellDeltaMv} mV` : '--');
        if (data.bms.cellMinMv != null && data.bms.cellMaxMv != null) {
            setText('bmsCells', `${data.bms.cellMinMv} \u2013 ${data.bms.cellMaxMv} mV`);
        } else {
            setText('bmsCells', '--');
        }
    } else {
        bmsCard.style.display = 'none';
    }
```

- [ ] **Step 4: Commit**

```bash
git add src/WebServer/Pages/TelemetryPage.h
git commit -m "feat: update telemetry JS for armed pill, CC/voltage swap and BMS split"
```

---

### Task 7: JS — Update wake lock UI wiring

**Files:**
- Modify: `src/WebServer/Pages/TelemetryPage.h` (the `TELEMETRY_PAGE_JS` section)

- [ ] **Step 1: Replace wakeHelpToggle handler with wakeIconBtn handler in `initTelemetryWake`**

Find:
```js
    const helpToggle = $('wakeHelpToggle');
    const helpPanel = $('wakeHelp');
    if (helpToggle && helpPanel) {
        helpToggle.addEventListener('click', () => {
            const isOpen = helpPanel.classList.toggle('open');
            helpToggle.setAttribute('aria-expanded', isOpen ? 'true' : 'false');
        });
    }
```

Replace with:
```js
    const wakeIconBtn = $('wakeIconBtn');
    const wakePanel = $('wakePanel');
    if (wakeIconBtn && wakePanel) {
        wakeIconBtn.addEventListener('click', () => {
            const isOpen = wakePanel.classList.toggle('open');
            wakeIconBtn.setAttribute('aria-expanded', isOpen ? 'true' : 'false');
        });
    }
```

- [ ] **Step 2: Commit**

```bash
git add src/WebServer/Pages/TelemetryPage.h
git commit -m "feat: wire wake lock panel toggle to status bar icon"
```

---

### Task 8: Build and create PR

**Files:** None changed — verification only.

- [ ] **Step 1: Build all three targets**

```bash
~/.platformio/penv/bin/pio run -e lolin_c3_mini_hobbywing
~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor
~/.platformio/penv/bin/pio run -e lolin_c3_mini_xag
```

Expected: `SUCCESS` for all three with no errors or warnings in the changed files.

- [ ] **Step 2: Create PR**

```bash
gh pr create \
  --base main \
  --title "feat: telemetry UI redesign — portrait layout, larger text, armed pill" \
  --body "$(cat <<'EOF'
## Summary

- Replaces two hero cards (~192px) with a single 36px status bar, making room for all 7 data cards in portrait without scroll
- Topbar changed to single scrollable row (fixes "Configurações" wrapping to second line)
- Sistema card removed — armed/disarmed moved to Tensão card as color-coded pill (red = armed/danger, gray = disarmed)
- Sub-text increased from 11px to 17px (portrait) / 14px (landscape) — readable in direct sunlight
- Sub-text for compound data uses 2-line pattern (label + value)
- Bateria card: Coulomb Count as main value, voltage percentage as sub
- BMS card: Delta on first line, cell range on second line (no more truncation)
- Wake lock panel collapsed by default, expands via 🔒 icon in status bar

## Test plan

- [ ] Build succeeds for all three targets (`hobbywing`, `tmotor`, `xag`)
- [ ] Open telemetry page on iPhone in portrait — all 7 cards visible without scroll
- [ ] Open telemetry page on iPhone in landscape — all 7 cards visible in 4 columns
- [ ] Arm the controller — pill turns red with "ARMADO"
- [ ] Disarm — pill turns gray with "DESARMADO"
- [ ] Tap 🔒 icon — wake panel expands; tap again — collapses
- [ ] BMS connected — Delta and cell range appear on separate lines

🤖 Generated with [Claude Code](https://claude.com/claude-code)
EOF
)"
```

---
