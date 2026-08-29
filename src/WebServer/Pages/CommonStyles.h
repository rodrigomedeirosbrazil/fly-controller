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

.flight-time {
    display: flex;
    flex-direction: column;
    align-items: flex-start;
    gap: 2px;
    margin-top: 6px;
    white-space: normal;
    overflow: visible;
}

.ft-label {
    font-size: 11px;
    font-weight: 600;
    text-transform: uppercase;
    letter-spacing: 0.04em;
    opacity: 0.8;
}

.ft-value {
    font-size: 18px;
    font-weight: 700;
    line-height: 1.1;
    color: var(--text);
    white-space: nowrap;
}

.flight-time .btn-sm {
    font-size: 12px;
    padding: 2px 10px;
    margin-top: 3px;
}

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

input[type="number"],
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


.telemetry-grid .card.bms-card {
    justify-content: flex-start;
}

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
    gap: 10px;
}

.wake-icon-btn {
    width: 30px;
    height: 30px;
    border-radius: 8px;
    background: var(--card);
    border: 1px solid var(--border);
    color: var(--muted);
    cursor: pointer;
    font-size: 15px;
    line-height: 1;
    padding: 0;
    display: flex;
    align-items: center;
    justify-content: center;
}

.wake-icon-btn.sound-muted {
    opacity: 0.55;
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

.power-alert-panel {
    display: none;
    background: var(--card);
    border-radius: var(--radius);
    padding: 10px 12px;
    flex-shrink: 0;
    box-shadow: var(--shadow);
    font-size: 13px;
    line-height: 1.4;
    border-left: 4px solid var(--danger);
}

.power-alert-panel.open {
    display: flex;
    align-items: flex-start;
    gap: 10px;
}

.power-alert-title {
    font-weight: 700;
    color: var(--danger-text);
    font-size: 13px;
}

.power-alert-causes {
    color: var(--danger-mute);
    font-size: 12px;
    margin-top: 2px;
}

.power-alert-close {
    margin-left: auto;
    background: transparent;
    border: 0;
    color: var(--danger-mute);
    cursor: pointer;
    padding: 0 2px;
    font-size: 16px;
    line-height: 1;
    flex-shrink: 0;
}

.card.power-limit-active {
    background: var(--danger-bg);
    border: 1px solid var(--danger-border);
}

.card.power-limit-active .label {
    color: var(--danger-text);
}

.card.power-limit-active .value {
    color: var(--danger);
}

.power-limit-badge,
.sub2-value.power-limit-badge {
    /* .sub2-value is a block: without fit-content the badge paints as a
       full-width red bar instead of a chip. The second selector is not
       redundant — .sub2-value sets its own colour and is declared later in
       this file, so at equal specificity it would win and paint muted grey
       text on the red chip. */
    display: inline-block;
    width: fit-content;
    background: var(--danger);
    color: var(--on-danger);
    font-weight: 700;
    padding: 1px 6px;
    border-radius: 4px;
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
    background: rgba(255, 95, 82, 0.14);
    color: var(--danger);
}

.armed-pill.armed .armed-dot {
    background: var(--danger);
}

.armed-pill.disarmed {
    background: var(--surface-2);
    color: var(--muted);
}

.armed-pill.disarmed .armed-dot {
    background: var(--dot-off);
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

@media (max-width: 600px) {
    .page { padding: 14px; }
    .nav-btn { flex: 1 1 auto; text-align: center; }
    .grid { grid-template-columns: repeat(auto-fit, minmax(180px, 1fr)); }
}

/* Telemetry: fullscreen dashboard on mobile (portrait and landscape) */
@media (max-width: 768px), ((max-height: 500px) and (max-width: 1024px)) {
    html:has(body.telemetry-page) {
        height: 100%;
        min-height: 100dvh;
        overflow: hidden;
    }

    body.telemetry-page {
        min-height: 100dvh;
        height: 100%;
        overflow: hidden;
    }

    body.telemetry-page .page.page-telemetry {
        max-width: none;
        width: 100%;
        margin: 0;
        min-height: 100%;
        min-height: 100dvh;
        height: 100%;
        padding: 7px 9px 8px;
        display: flex;
        flex-direction: column;
        box-sizing: border-box;
    }

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

    body.telemetry-page .page.page-telemetry .topbar .nav-btn {
        padding: 7px 8px;
        font-size: 12px;
    }

    .telemetry-shell {
        flex: 1;
        min-height: 0;
        display: flex;
        flex-direction: column;
        gap: 6px;
        overflow: hidden;
    }

    .telemetry-status-bar {
        height: 34px;
        padding: 0 10px;
    }

    .wake-icon-btn {
        width: 26px;
        height: 26px;
        font-size: 13px;
    }

    .telemetry-grid.grid {
        flex: 1;
        min-height: 0;
        overflow: hidden;
        display: grid;
        gap: 8px;
        align-content: stretch;
        grid-template-columns: repeat(2, minmax(0, 1fr));
        grid-auto-rows: minmax(0, 1fr);
    }

    .telemetry-grid .card {
        min-height: 0;
        min-width: 0;
        padding: 10px 11px 9px;
        display: flex;
        flex-direction: column;
        justify-content: flex-start;
        overflow: hidden;
    }

    .telemetry-grid .label {
        font-size: 11px;
        font-weight: 600;
        line-height: 1.05;
    }

    /* The signal-validity badge sits inline in the label on cards that can
       show one (battery/motor/ESC temp). At the base .status size (12px +
       4px/10px padding) it's taller than the label and can wrap the label
       onto a second line, pushing .value/.sub into the card's overflow:
       hidden clip — precisely during the fault it exists to announce. */
    .telemetry-grid .label .status {
        font-size: 9px;
        padding: 2px 6px;
        white-space: nowrap;
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
}
)rawliteral";
