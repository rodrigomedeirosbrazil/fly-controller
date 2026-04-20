# Telemetry UI Redesign — Design Spec

**Date:** 2026-04-20  
**Branch:** feat/log-timestamp  
**Files affected:** `src/WebServer/Pages/TelemetryPage.h`, `src/WebServer/Pages/CommonStyles.h`

---

## Problem

The telemetry screen in portrait orientation (iPhone/Android) has three compounding issues:

1. **Topbar wraps to 2 rows** — "Configurações" overflows, consuming ~32px of extra height.
2. **Two hero cards consume ~192px** — "Telemetria ao Vivo" and "Manter Tela Ligada" as full cards take roughly 30% of the screen before any data appears.
3. **Text is too small and gets cut** — `.sub` at 11px is unreadable in sunlight; "DESARMADO" is truncated to "DESARM…"; BMS cell data is crammed into one line and clipped.

Combined, only 3.5 rows of data cards are visible in portrait, and the Sistema and BMS cards are partially or fully hidden.

---

## Goals

- All 7 data cards visible simultaneously in portrait with no scroll.
- Sub-text legible in direct sunlight.
- Armed/disarmed state immediately visible and color-coded for safety.
- BMS cell data fully readable without truncation.
- Wake lock control accessible but not taking up permanent space.

---

## Design Decisions

### 1. Topbar — single scrollable row

**Before:** `flex-wrap: wrap` — "Configurações" breaks to a second row (~56px total).  
**After:** `flex-wrap: nowrap; overflow-x: auto` — single row (~32px). "Configurações" abbreviated to "Config." as nav label.

### 2. Hero cards → thin status bar

Replace the two full-height cards with a single 36px bar:

```
[ Telemetria   AO VIVO          🔒 ]
```

- Left: label "TELEMETRIA"
- Center: data status badge (AO VIVO / DESATUALIZADO / SEM DADOS)
- Right: wake lock icon button 🔒

**Wake lock panel:** hidden by default. Tapping 🔒 expands a compact panel below the status bar showing the current state badge and a toggle button. Auto wake lock still runs on page load — the panel is only needed as a fallback.

Space saved: ~156px in portrait (from ~192px to ~36px).

### 3. Card Sistema removed — armed state merged into Tensão card

The "Sistema" card is eliminated. Armed/disarmed state moves to the Tensão (voltage) card as a pill badge below the main value.

**Color coding:**
- `ARMADO` — red background (`#fee2e2`), red text (`#b91c1c`) — signals danger (motor can spin).
- `DESARMADO` — gray background (`#f3f4f6`), gray text (`#6b7280`) — neutral/safe state.

The freshness timestamp ("Última atualização há X ms") is removed from the visible UI — it's low-priority information and the AO VIVO / DESATUALIZADO badge already communicates staleness.

### 4. Card Bateria (formerly SoC Bateria)

- **Label:** "BATERIA" (was "SOC BATERIA")
- **Main value:** Coulomb Count percentage (`batteryPercentCc`) — more accurate.
- **Sub-text:** Voltage-based percentage (`batteryPercentVoltage`) with label "Tensão".

### 5. Typography — all sizes increased

| Element | Before | After |
|---|---|---|
| `.label` | 10–11px | 11px + `font-weight: 600` |
| `.value` (portrait) | `clamp(27px, 4.4vw, 36px)` | `clamp(22px, 6vw, 34px)` |
| `.sub` (portrait) | `clamp(14px, 2.3vw, 19px)` → effectively ~11px | 17px fixed (portrait) / 14px (landscape) |

### 6. Sub-text structure — 2-line pattern for compound data

Cards with a label+value secondary datum use a two-line sub structure:

```
LABEL (10px uppercase muted)
value (17px muted bold)
```

Applied to: **Bateria** (Tensão %), **Energia** (Limite), **Acelerador** (Bruto).  
Not applied to: **Motor** (RPM — single value), **ESC** (current — single value).

### 7. BMS card — two plain sub lines, specific order

```
BMS
31.2 C
Delta: 50 mV       ← first sub line (plain .sub)
3800 – 3850 mV     ← second sub line (plain .sub)
```

Delta is shown first (more actionable — indicates cell imbalance). Cell range is shown second.

### 8. Grid layout

**Portrait (≤768px):**
- 2-column grid
- BMS card: `grid-column: 1 / -1` (full width) when visible

**Landscape (≤1024px, ≤500px height):**
- 4-column grid (unchanged)
- BMS card: `grid-column: span 2` when visible

---

## Card Inventory

| Card | Label | Main value | Sub line 1 | Sub line 2 |
|---|---|---|---|---|
| Tensão | TENSÃO | `batteryVoltageMv` formatted | Armed pill (ARMADO/DESARMADO) | — |
| Bateria | BATERIA | `batteryPercentCc` + % | "Tensão" label | `batteryPercentVoltage` + % |
| Energia | ENERGIA | `powerKwX10` formatted | "Limite" label | `powerPercent` + % |
| Acelerador | ACELERADOR | `throttlePercent` + % | "Bruto" label | `throttleRaw` |
| Motor | MOTOR | `motorTempMc` formatted | `rpm` + "rpm" (or N/A) | — |
| ESC | ESC | `escTempMc` formatted | `escCurrentMa` formatted (or N/A) | — |
| BMS *(optional)* | BMS | `bms.tempMaxC` + C | "Delta: X mV" | "XXXX – XXXX mV" |

The Sistema card and its freshness timestamp are removed from the UI entirely.

---

## Files to Change

### `src/WebServer/Pages/TelemetryPage.h`

1. **HTML structure:**
   - Replace `.telemetry-hero-grid` (two `.hero-card` panels) with a single `.status-bar` div.
   - Add a `.wake-panel` div (hidden by default) below the status bar.
   - Remove the Sistema card (`id="armed"`, `id="freshness"`).
   - Add armed pill markup inside the Tensão card.
   - Update Bateria card: label, swap main/sub IDs, add 2-line sub structure.
   - Update Energia, Acelerador cards to 2-line sub structure.
   - Update BMS card: two `.sub` lines — `id="bmsDelta"` (new, first line) and `id="bmsCells"` (repurposed from combined text to cell range only).

2. **JavaScript (`TELEMETRY_PAGE_JS`):**
   - Remove `freshness` setText call.
   - Change `armed` rendering: set pill class (`armed`/`disarmed`) and text instead of setting text on a `.value`.
   - Swap `batteryPercentCc` ↔ `batteryPercentVoltage` assignments (CC → main value, voltage → sub).
   - Update BMS rendering: split `bmsDelta` and `bmsCells` into separate setText calls.
   - Wake lock panel: toggle `.wake-panel` visibility on 🔒 icon click.

### `src/WebServer/Pages/CommonStyles.h`

1. **Topbar (mobile):** change `flex-wrap: wrap` to `flex-wrap: nowrap; overflow-x: auto; -webkit-overflow-scrolling: touch; scrollbar-width: none`.
2. **Status bar:** new `.status-bar` rule (height 36px, flex row, card background).
3. **Wake panel:** new `.wake-panel` rule (hidden by default, `.wake-panel.open` shows it).
4. **Hero cards:** remove `.telemetry-hero-grid`, `.hero-card`, `.hero-main`, `.hero-actions`, `.wake-card`, `.telemetry-header` rules.
5. **Typography (mobile media query):**
   - `.telemetry-grid .label`: 11px, font-weight 600
   - `.telemetry-grid .value`: `clamp(22px, 6vw, 34px)`
   - `.telemetry-grid .sub`: 17px, font-weight 500
6. **Sub 2-line pattern:** `.sub2`, `.sub2-label` (10px uppercase muted), `.sub2-value` (17px muted bold).
7. **Armed pill:** `.armed-pill` base, `.armed-pill.armed` (red), `.armed-pill.disarmed` (gray).
8. **BMS card:** `.sub + .sub` margin rule (2px gap between consecutive sub lines).
9. **Portrait BMS:** `grid-column: 1 / -1`.
10. **Landscape BMS:** `grid-column: span 2`.
11. **Desktop (min-width 769px):** update `.sub` from 15px to 17px.

---

## Out of Scope

- No changes to the telemetry API (`/api/telemetry` endpoint).
- No changes to other pages (Dashboard, Config, etc.).
- No changes to wake lock logic — only its UI container changes.
- No changes to data availability flags (`av.rpm`, `av.current`, etc.).
