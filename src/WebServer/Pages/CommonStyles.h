#pragma once

const char* COMMON_CSS = R"rawliteral(
:root {
    /* Dark instrument-panel palette. The phone is on the controller's AP in
       daylight: a light theme loses contrast in the sun and burns OLED. */
    --bg: #0a0e13;
    --card: #141a21;
    --card-sunken: #11161c;
    --text: #e8eef3;
    --muted: #8496a4;
    --dim: #556575;
    --border: #232c36;
    --border-strong: #2b3641;
    --track: #202932;

    --primary: #4fb8ff;
    --primary-bg: #16324a;
    --primary-border: #245580;
    --on-primary: #06121c;

    --ok: #3fd88b;
    --warn: #f5b13d;
    --danger: #ff5f52;
    --danger-bg: #26100e;
    --danger-border: #d0453a;
    --danger-text: #ff9d94;

    --zone: #4a1f1c;
    --zone-hot: #7a2a24;
    --track-dead: #2a2328;
    --fault-bg: #1c1417;
    --fault-border: #4a3238;
    --surface-2: #1c232b;
    --dot-off: #55636f;
    --on-danger: #1a0a09;
    --danger-mute: #d09a94;

    --shadow: 0 2px 8px rgba(0, 0, 0, 0.45);
    --radius: 12px;
}

* { box-sizing: border-box; }

body {
    /* No webfonts: the phone is on the AP with no internet, so a Google Fonts
       <link> would just stall the load. tabular-nums keeps the digits from
       shifting on every 1 Hz telemetry refresh. */
    font-family: system-ui, -apple-system, "Segoe UI", sans-serif;
    font-variant-numeric: tabular-nums;
    -webkit-font-smoothing: antialiased;
    margin: 0;
    background: var(--bg);
    color: var(--text);
}

.page {
    max-width: 980px;
    margin: 0 auto;
    padding: 20px;
}

.page.narrow {
    max-width: 760px;
}

.topbar {
    position: sticky;
    top: 0;
    z-index: 10;
    display: flex;
    flex-wrap: wrap;
    gap: 10px;
    margin-bottom: 16px;
    padding: 10px 0 12px;
    background: var(--bg);
}

.subnav {
    display: flex;
    flex-wrap: wrap;
    gap: 10px;
    margin-bottom: 16px;
}

.nav-btn {
    background: var(--card);
    border: 1px solid var(--border);
    color: var(--muted);
    text-decoration: none;
    border-radius: 8px;
    padding: 10px 14px;
    font-weight: 600;
    font-size: 14px;
}

.nav-btn.active {
    background: var(--primary-bg);
    border-color: var(--primary-border);
    color: var(--primary);
}

.panel {
    background: var(--card);
    border-radius: var(--radius);
    padding: 16px;
    box-shadow: var(--shadow);
    margin-bottom: 16px;
}

.card {
    background: var(--card);
    border-radius: var(--radius);
    padding: 14px;
    box-shadow: var(--shadow);
}

.link-card {
    color: inherit;
    text-decoration: none;
    border: 1px solid var(--border);
    transition: transform 0.15s ease, box-shadow 0.15s ease;
}

.link-card:hover {
    transform: translateY(-1px);
    border-color: var(--border-strong);
    box-shadow: 0 6px 16px rgba(0, 0, 0, 0.5);
}

.grid {
    display: grid;
    gap: 12px;
    grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
}

.label {
    color: var(--muted);
    font-size: 12px;
    text-transform: uppercase;
    letter-spacing: 0.04em;
}

.value {
    font-size: 20px;
    font-weight: bold;
    margin-top: 4px;
}

.sub {
    color: var(--muted);
    margin-top: 4px;
    font-size: 13px;
}

h1 {
    margin: 0 0 8px 0;
    font-size: 24px;
}

h2 {
    color: var(--muted);
    border-bottom: 1px solid var(--border);
    padding-bottom: 10px;
    margin-top: 24px;
    font-size: 18px;
}

form { margin: 0; }

.form-group { margin-bottom: 16px; }

label {
    display: block;
    margin-bottom: 6px;
    color: var(--muted);
    font-weight: 600;
}

.info-text {
    color: var(--dim);
    font-size: 12px;
    margin-top: 6px;
}

.total-voltage {
    background-color: rgba(79, 184, 255, 0.10);
    padding: 8px;
    border-radius: 6px;
    margin-top: 6px;
    font-size: 12px;
    color: var(--primary);
}

/* Every text-like input, not just number: the PIN field is type="password"
   and sits on every config page, so leaving it out meant a white box on a
   dark page. */
input[type="number"],
input[type="text"],
input[type="password"],
select,
input[type="file"] {
    width: 100%;
    padding: 10px;
    background: var(--card-sunken);
    border: 1px solid var(--border-strong);
    border-radius: 8px;
    color: var(--text);
    font-family: inherit;
    font-size: 14px;
}

button,
.btn {
    background: var(--primary);
    color: var(--on-primary);
    border: 0;
    border-radius: 8px;
    padding: 12px;
    cursor: pointer;
    font-family: inherit;
    font-weight: 700;
    font-size: 16px;
}

button:disabled {
    background: var(--surface-2);
    color: var(--dim);
    cursor: not-allowed;
}

/* Native controls: one line instead of restyling them by hand. */
input[type="range"],
input[type="checkbox"] {
    accent-color: var(--primary);
}

input::placeholder { color: var(--dim); }

.message {
    margin-top: 12px;
    padding: 10px;
    border-radius: 6px;
    display: none;
}

.message.ok {
    background: rgba(63, 216, 139, 0.12);
    color: var(--ok);
}

.message.err {
    background: rgba(255, 95, 82, 0.12);
    color: var(--danger);
}

.table-wrap { overflow-x: auto; }

table {
    width: 100%;
    border-collapse: collapse;
    margin-top: 12px;
    min-width: 520px;
}

th,
td {
    text-align: left;
    padding: 10px;
    border-bottom: 1px solid var(--border);
}

th {
    color: var(--muted);
    font-size: 12px;
    text-transform: uppercase;
}

.btn-sm {
    padding: 6px 10px;
    font-size: 12px;
    border-radius: 6px;
}

.btn-green { background: var(--ok); color: var(--on-primary); }
.btn-red { background: var(--danger); color: var(--on-danger); }

.status {
    display: inline-flex;
    align-items: center;
    justify-content: center;
    padding: 4px 10px;
    border-radius: 999px;
    font-size: 12px;
    font-weight: bold;
    line-height: 1;
    vertical-align: middle;
}

.status.live { background: rgba(63, 216, 139, 0.12); color: var(--ok); }
.status.stale { background: rgba(245, 177, 61, 0.14); color: var(--warn); }
.status.nodata { background: rgba(255, 95, 82, 0.14); color: var(--danger); }
.status.status-secondary { background: var(--surface-2); color: var(--muted); }
.status.status-active { background: rgba(63, 216, 139, 0.12); color: var(--ok); }
.status.status-warning { background: rgba(245, 177, 61, 0.14); color: var(--warn); }
.status.status-inactive { background: rgba(255, 95, 82, 0.14); color: var(--danger); }

@media (max-width: 600px) {
    .page { padding: 14px; }
    .nav-btn { flex: 1 1 auto; text-align: center; }
    .grid { grid-template-columns: repeat(auto-fit, minmax(180px, 1fr)); }
}
/* ============================================================================
   Telemetry panel.

   Six bands in a fixed vertical order, filling the viewport: status, battery,
   instruments, throttle, drawer handle, navigation. Nothing is ever added to
   or removed from this stack -- only the content and colour inside a band
   change -- so an alert can never move a number the pilot is reading.

   Sizes are fr and clamp(), never fixed px inside a media query, and there is
   exactly one media query: orientation. The old four-breakpoint block relied
   on overflow:hidden to survive, which clipped the value instead of resizing
   it. Nothing here clips.
   ============================================================================ */

html:has(body.telemetry-page) { height: 100%; overflow: hidden; }

body.telemetry-page {
    height: 100dvh;
    overflow: hidden;
    overscroll-behavior: none;
}

.tp {
    /* Padding, gaps and the fixed bands are all clamp()ed against vh so the
       panel compresses continuously on a short viewport instead of needing a
       height breakpoint. Without this, a 320x480 screen starved the battery
       dial to nothing while everything else kept its full size. */
    height: 100dvh;
    display: flex;
    flex-direction: column;
    gap: clamp(5px, 1.2vh, 10px);
    padding: clamp(6px, 1.5vh, 12px);
    box-sizing: border-box;
}

.tp-main {
    flex: 1;
    min-width: 0;
    min-height: 0;
    display: flex;
    flex-direction: column;
    gap: clamp(5px, 1.2vh, 10px);
}

/* --- band 1: status ------------------------------------------------------ */

.tp-status {
    height: clamp(28px, 4.6vh, 34px);
    flex-shrink: 0;
    display: flex;
    align-items: center;
    gap: 5px;
    padding: 0 2px;
    min-width: 0;
}

.tp-spacer { flex-grow: 1; }

.tp-pill {
    /* The status bar must fit two pills, the clock and two icons at 360 px
       without wrapping or pushing the lock button off-screen, so the pills
       shrink and ellipsis before anything leaves the bar. */
    display: inline-flex;
    align-items: center;
    gap: 5px;
    min-width: 0;
    border-radius: 999px;
    padding: 5px 9px;
    font-size: clamp(9px, 2.7vw, 11px);
    font-weight: 800;
    letter-spacing: 0.06em;
    white-space: nowrap;
    overflow: hidden;
    border: 0;
}

.tp-pill > span:not(.tp-dot) {
    overflow: hidden;
    text-overflow: ellipsis;
}

.tp-dot {
    width: 7px;
    height: 7px;
    border-radius: 50%;
    display: block;
    flex-shrink: 0;
}

/* Live is the boring state: the green dot alone says it, and dropping the
   label is what lets the fault chip fit at 360 px. Degraded states keep
   their words. */
.tp-pill.ok > span:not(.tp-dot) { display: none; }
.tp-pill.ok { background: rgba(63, 216, 139, 0.12); color: var(--ok); padding: 5px 7px; }
.tp-pill.ok .tp-dot { background: var(--ok); }
.tp-pill.warn { background: rgba(245, 177, 61, 0.14); color: var(--warn); }
.tp-pill.warn .tp-dot { background: var(--warn); }
.tp-pill.danger { background: rgba(255, 95, 82, 0.14); color: var(--danger); }
.tp-pill.danger .tp-dot { background: var(--danger); }
.tp-pill.off { background: var(--surface-2); color: var(--muted); }
.tp-pill.off .tp-dot { background: var(--dot-off); }
.tp-pill.alarm { background: var(--danger); color: var(--on-danger); cursor: pointer; }

.tp-clock {
    font-weight: 700;
    font-size: clamp(15px, 4.4vw, 19px);
    letter-spacing: -0.01em;
    white-space: nowrap;
    flex-shrink: 0;
}

.tp-icon {
    width: 28px;
    height: 28px;
    border-radius: 8px;
    border: 1px solid var(--border);
    background: var(--card);
    color: var(--muted);
    padding: 0;
    flex-shrink: 0;
    display: flex;
    align-items: center;
    justify-content: center;
    cursor: pointer;
}

.tp-icon.off { opacity: 0.5; }
.tp-icon.on { border-color: var(--primary-border); background: var(--primary-bg); color: var(--primary); }

/* --- shared tile + type -------------------------------------------------- */

.tp-tile {
    background: var(--card);
    border: 1px solid var(--border);
    border-radius: 14px;
    display: flex;
    flex-direction: column;
    min-width: 0;
    min-height: 0;
}

.tp-tile.limiting { background: var(--danger-bg); border-color: var(--danger-border); }
.tp-tile.faulted { background: var(--fault-bg); border-color: var(--fault-border); }

.tp-lab {
    font-size: 11px;
    font-weight: 700;
    letter-spacing: 0.12em;
    text-transform: uppercase;
    color: var(--muted);
    line-height: 1;
    white-space: nowrap;
}

.tp-tile.limiting .tp-lab { color: var(--danger-text); }
.tp-tile.faulted .tp-lab { color: var(--danger-mute); }

.tp-num {
    font-weight: 700;
    letter-spacing: -0.02em;
    line-height: 0.95;
}

.tp-unit {
    color: var(--muted);
    font-weight: 600;
}

.tp-chip {
    font-size: 9px;
    font-weight: 800;
    letter-spacing: 0.07em;
    color: var(--on-danger);
    background: var(--danger);
    border-radius: 4px;
    padding: 3px 6px;
    white-space: nowrap;
}

/* --- band 2: battery ----------------------------------------------------- */

/* The battery is the primary instrument, so it is the band that absorbs
   leftover height. The instrument row takes only what its dials need --
   giving it the slack just left three tall, mostly empty tiles. */
.tp-hero {
    flex: 1 1 auto;
    min-height: 0;
    align-items: center;
    justify-content: center;
    padding: clamp(8px, 1.8vh, 14px);
    gap: 4px;
}

/* Instruments are SVG with a viewBox: they have no size in pixels, only a
   proportion. Portrait scales them by width, landscape by height. That is
   what makes the panel survive an orientation change without a new rule.

   The arc is a 270 deg sweep: the group is rotated -135 deg so the gap sits
   at the bottom, and the track length is 0.75 * circumference. A value arc
   is that length times value/max; the reduction zone is the same with a
   negative dash offset. Everything else is one setAttribute per poll. */

.tp-gauge {
    display: block;
    width: 100%;
    height: auto;
}

/* Same treatment as the instrument dials: the SVG letterboxes inside whatever
   box the band can spare. Left as a plain width:100% block it was a flex item
   with height derived from width, and on a short viewport (320x480) the column
   squashed it to nothing -- the primary instrument vanished while the rest of
   the panel still fit. */
.tp-gauge-batt {
    flex: 1 1 auto;
    min-height: 48px;
    width: 100%;
    height: 100%;
    max-width: 100%;
}

/* The instrument row is the flexible band, so let the gauge take the height
   it is given instead of leaving it empty: the viewBox letterboxes on its
   own, so this only ever grows the dial, never distorts it. */
.tp-inst .tp-gauge {
    flex: 1 1 auto;
    min-height: 0;
    width: 100%;
    height: 100%;
}

.tp-gauge circle { fill: none; }
.tp-gauge .g-track { stroke: var(--track); stroke-linecap: round; }
.tp-gauge .g-zone { stroke: var(--zone); }
.tp-gauge .g-val { stroke: var(--ok); stroke-linecap: round; }

.tp-gauge text { font-weight: 700; }
.tp-gauge .g-num { fill: var(--text); letter-spacing: -1.5px; }
.tp-gauge .g-unit { fill: var(--muted); letter-spacing: 1px; }
.tp-gauge .g-cap { fill: var(--muted); letter-spacing: 3.5px; }
.tp-gauge .g-sensor { fill: var(--dim); letter-spacing: 2.5px; }
.tp-gauge .g-end { fill: var(--dim); }

.tp-tile.limiting .g-val { stroke: var(--danger); }
.tp-tile.limiting .g-zone { stroke: var(--zone-hot); }
.tp-tile.limiting .g-num { fill: var(--danger); }

/* A signal we do not trust draws no arc at all -- a dimmed track and an em
   dash, never a number the pilot might act on. */
.tp-tile.faulted .g-track { stroke: var(--track-dead); }
.tp-tile.faulted .g-zone,
.tp-tile.faulted .g-val { display: none; }
.tp-tile.faulted .g-num,
.tp-tile.faulted .g-unit,
.tp-tile.faulted .g-sensor { fill: #8a6f74; }

.tp-sep {
    width: 100%;
    height: 1px;
    flex-shrink: 0;
    background: var(--border);
    margin-top: clamp(5px, 1.2vh, 10px);
}

.tp-cells {
    width: 100%;
    flex-shrink: 0;
    display: grid;
    grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: 11px;
    padding-top: clamp(6px, 1.4vh, 10px);
}

/* A missing source removes its cell; the row centres what is left instead of
   leaving a placeholder. */
.tp-cells.single { grid-template-columns: minmax(0, 1fr); }

.tp-cell { text-align: center; min-width: 0; }

/* The voltage cell toggles between pack total and per-cell. Reset the button
   chrome so it stays visually identical to the plain cell next to it -- the
   swap glyph in the label is the only affordance, which is what a panel read
   at a glance wants. */
.tp-cell-btn {
    background: none;
    border: 0;
    padding: 0;
    font: inherit;
    color: inherit;
    width: 100%;
    cursor: pointer;
    -webkit-tap-highlight-color: transparent;
}

.tp-cell-btn .tp-lab svg { vertical-align: -1px; opacity: 0.6; }
.tp-cell-btn:active { opacity: 0.6; }
.tp-cell + .tp-cell { border-left: 1px solid var(--border); }
.tp-cell .tp-lab { font-size: 10px; letter-spacing: 0.08em; }
.tp-cell .tp-num { font-size: clamp(22px, 8vmin, 30px); margin-top: 9px; }
.tp-cell .tp-unit { font-size: 14px; }

/* --- band 3: instruments ------------------------------------------------- */

.tp-instruments {
    flex: 0 0 auto;
    min-height: 0;
    display: grid;
    grid-template-columns: repeat(3, minmax(0, 1fr));
    gap: 10px;
}

/* Power has no full scale, so it is a readout, not an instrument. When the
   ESC reports no power at all the tile goes away and the row closes up. */
.tp-instruments.cols-2 { grid-template-columns: repeat(2, minmax(0, 1fr)); }

.tp-inst {
    align-items: center;
    justify-content: center;
    gap: 10px;
    padding: 10px 6px 12px;
}

.tp-inst .tp-num { font-size: clamp(28px, 10vmin, 46px); }
.tp-inst .tp-unit { font-size: 13px; letter-spacing: 0.18em; }
.tp-inst.limiting .tp-num { color: var(--danger); }
.tp-inst.faulted .tp-num { color: var(--danger-mute); }

/* --- band 4: throttle ---------------------------------------------------- */

.tp-throttle {
    flex-shrink: 0;
    display: grid;
    grid-template-columns: 1fr auto;
    align-items: center;
    gap: clamp(5px, 1.2vh, 9px) 8px;
    padding: clamp(7px, 1.5vh, 11px) 14px clamp(8px, 1.7vh, 13px);
}

.tp-throttle .tp-lab { grid-area: 1 / 1 / 2 / 2; }
.tp-thr-val { grid-area: 1 / 2 / 2 / 3; font-size: 22px; color: var(--primary); }
.tp-throttle .tp-bar { grid-area: 2 / 1 / 3 / 3; }

.tp-bar {
    height: 14px;
    border-radius: 7px;
    background: var(--track);
    position: relative;
}

.tp-bar > i {
    display: block;
    height: 100%;
    border-radius: 7px;
    background: var(--primary);
}

/* Marks where the power ceiling cuts in, so pushing past it makes sense. */
.tp-cap {
    position: absolute;
    top: -3px;
    bottom: -3px;
    width: 2px;
    border-radius: 1px;
    background: var(--danger);
    display: none;
}

/* --- band 5: drawer handle ----------------------------------------------- */

.tp-more {
    height: clamp(36px, 6vh, 46px);
    flex-shrink: 0;
    background: var(--card-sunken);
    border: 1px solid var(--border);
    border-radius: 12px;
    display: flex;
    align-items: center;
    gap: 10px;
    padding: 0 14px;
    width: 100%;
    color: inherit;
    font: inherit;
    cursor: pointer;
}

.tp-more-hint { color: var(--dim); overflow: hidden; text-overflow: ellipsis; }

/* --- band 6: navigation -------------------------------------------------- */

.tp-nav {
    height: clamp(34px, 5.5vh, 42px);
    flex-shrink: 0;
    display: grid;
    grid-template-columns: repeat(5, minmax(0, 1fr));
    gap: 6px;
}

.tp-nav a {
    display: flex;
    align-items: center;
    justify-content: center;
    background: var(--card);
    border: 1px solid var(--border);
    border-radius: 10px;
    color: var(--muted);
    text-decoration: none;
    font-size: 12px;
    font-weight: 700;
}

.tp-nav a.active {
    background: var(--primary-bg);
    border-color: var(--primary-border);
    color: var(--primary);
}

/* --- drawer (overlay, never part of the six bands) ----------------------- */

.tp-scrim {
    position: fixed;
    inset: 0;
    z-index: 19;
    background: rgba(5, 8, 11, 0.55);
    opacity: 0;
    pointer-events: none;
    transition: opacity 0.18s ease;
}

.tp-scrim.open { opacity: 1; pointer-events: auto; }

.tp-drawer {
    position: fixed;
    left: 0;
    right: 0;
    bottom: 0;
    z-index: 20;
    max-height: 86dvh;
    background: var(--card-sunken);
    border-top: 1px solid var(--border-strong);
    border-radius: 20px 20px 0 0;
    box-shadow: 0 -18px 40px rgba(0, 0, 0, 0.55);
    display: flex;
    flex-direction: column;
    padding: 10px 18px 16px;
    transform: translateY(101%);
    transition: transform 0.18s ease;
}

.tp-drawer.open { transform: none; }

.tp-grab {
    width: 44px;
    height: 4px;
    border-radius: 2px;
    background: var(--border-strong);
    margin: 2px auto 10px;
    flex-shrink: 0;
}

.tp-drawer-head {
    display: flex;
    align-items: center;
    gap: 10px;
    flex-shrink: 0;
    padding-bottom: 6px;
}

.tp-fault {
    display: flex;
    align-items: flex-start;
    gap: 11px;
    background: var(--danger-bg);
    border: 1px solid var(--danger-border);
    border-radius: 12px;
    padding: 11px 12px;
    margin-top: 6px;
    flex-shrink: 0;
}

.tp-fault-title { font-size: 14px; font-weight: 700; color: var(--danger-text); }
.tp-fault-detail { font-size: 12.5px; color: var(--danger-mute); margin-top: 3px; line-height: 1.35; }

.tp-rows {
    flex: 1;
    overflow-y: auto;
    min-height: 0;
    /* Values are right-aligned; without the gutter the scrollbar sits on top
       of them. */
    padding-right: 10px;
    scrollbar-gutter: stable;
    -webkit-overflow-scrolling: touch;
}

.tp-sec {
    font-size: 10px;
    font-weight: 800;
    letter-spacing: 0.16em;
    text-transform: uppercase;
    color: var(--primary);
    margin-top: 16px;
}

.tp-row {
    display: flex;
    align-items: baseline;
    gap: 12px;
    padding: 11px 0;
    border-bottom: 1px solid var(--border);
}

.tp-row:last-child { border-bottom: 0; }
.tp-row .k { font-size: 14px; color: var(--muted); font-weight: 500; }
.tp-row .v { font-size: 19px; font-weight: 700; margin-left: auto; text-align: right; }

.tp-drawer-foot {
    display: flex;
    align-items: center;
    gap: 10px;
    flex-shrink: 0;
    padding-top: 12px;
    margin-top: auto;
    border-top: 1px solid var(--border);
}

/* --- the one media query ------------------------------------------------- */

@media (orientation: landscape) {
    .tp { flex-direction: row; padding: 10px; }

    .tp-nav {
        order: -1;
        width: 52px;
        height: auto;
        grid-template-columns: minmax(0, 1fr);
        grid-auto-rows: minmax(0, 1fr);
    }

    .tp-nav a { font-size: 10px; letter-spacing: 0.04em; }

    /* The bands become a 2-column grid: battery beside the instruments, the
       drawer handle folded into the status row, throttle spanning both. */
    .tp-main {
        display: grid;
        grid-template-columns: 1.5fr 2.6fr;
        grid-template-rows: 30px minmax(0, 1fr) 46px;
        gap: 8px;
    }

    /* The status row takes the full width -- squeezed into one column the
       pills ellipsised to "A...". The drawer handle collapses to an icon and
       floats at its right; the padding reserves the space so they never
       collide even though they share the grid cell. */
    .tp-status {
        grid-area: 1 / 1 / 2 / 3;
        height: auto;
        padding-right: 54px;
    }

    .tp-more {
        grid-area: 1 / 2 / 2 / 3;
        justify-self: end;
        align-self: stretch;
        width: 46px;
        height: auto;
        padding: 0;
        justify-content: center;
    }

    .tp-more .tp-lab { display: none; }
    .tp-hero { grid-area: 2 / 1 / 3 / 2; justify-content: center; }
    .tp-instruments { grid-area: 2 / 2 / 3 / 3; }

    .tp-throttle {
        grid-area: 3 / 1 / 4 / 3;
        grid-template-columns: auto 1fr auto;
        gap: 12px;
        padding: 0 14px;
    }

    .tp-throttle .tp-bar { grid-area: 1 / 2 / 2 / 3; height: 12px; }
    .tp-thr-val { grid-area: 1 / 3 / 2 / 4; }

    .tp-clock { font-size: 17px; }

    .tp-gauge { width: auto; height: 100%; max-width: 100%; margin: 0 auto; }
    .tp-hero { padding: 8px 12px 10px; }
}
)rawliteral";
