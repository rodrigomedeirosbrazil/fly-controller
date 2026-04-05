#pragma once

const char* COMMON_CSS = R"rawliteral(
:root {
    --bg: #eef2f6;
    --text: #1f2937;
    --muted: #6b7280;
    --primary: #0b74de;
    --card: #ffffff;
    --border: #e5e7eb;
    --shadow: 0 2px 8px rgba(0, 0, 0, 0.08);
    --radius: 10px;
}

* { box-sizing: border-box; }

body {
    font-family: Arial, sans-serif;
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
    background: var(--primary);
    color: white;
    text-decoration: none;
    border-radius: 8px;
    padding: 10px 14px;
    font-weight: 600;
    font-size: 14px;
}

.nav-btn.active {
    box-shadow: inset 0 0 0 2px rgba(255, 255, 255, 0.5);
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
    box-shadow: 0 6px 16px rgba(0, 0, 0, 0.12);
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

/* Telemetry page: larger cards on desktop only (avoid overriding mobile telemetry rules) */
@media (min-width: 769px) and (min-height: 501px) {
    .page-telemetry .telemetry-grid .label {
        font-size: 13px;
    }

    .page-telemetry .telemetry-grid .value {
        font-size: 24px;
    }

    .page-telemetry .telemetry-grid .sub {
        font-size: 15px;
    }
}

h1 {
    margin: 0 0 8px 0;
    font-size: 24px;
}

h2 {
    color: #4b5563;
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
    color: #4b5563;
    font-weight: 600;
}

.info-text {
    color: var(--muted);
    font-size: 12px;
    margin-top: 6px;
}

.total-voltage {
    background-color: #e7f3ff;
    padding: 8px;
    border-radius: 4px;
    margin-top: 6px;
    font-size: 12px;
    color: #0066cc;
}

input[type="number"],
select,
input[type="file"] {
    width: 100%;
    padding: 10px;
    border: 1px solid #d1d5db;
    border-radius: 6px;
    font-size: 14px;
}

button,
.btn {
    background: var(--primary);
    color: white;
    border: 0;
    border-radius: 6px;
    padding: 12px;
    cursor: pointer;
    font-size: 16px;
}

button:disabled {
    background: #cbd5e1;
    cursor: not-allowed;
}

.message {
    margin-top: 12px;
    padding: 10px;
    border-radius: 6px;
    display: none;
}

.message.ok {
    background: #dcfce7;
    color: #166534;
}

.message.err {
    background: #fee2e2;
    color: #991b1b;
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

.btn-green { background: #16a34a; }
.btn-red { background: #dc2626; }

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

.status.live { background: #dcfce7; color: #166534; }
.status.stale { background: #fef9c3; color: #854d0e; }
.status.nodata { background: #fee2e2; color: #991b1b; }
.status.status-secondary { background: #e5e7eb; color: #374151; }
.status.status-active { background: #dcfce7; color: #166534; }
.status.status-warning { background: #fef3c7; color: #92400e; }
.status.status-inactive { background: #fee2e2; color: #991b1b; }

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

.telemetry-grid .card.bms-card {
    justify-content: flex-start;
}

.telemetry-grid .sub.multiline {
    white-space: normal;
    overflow: visible;
    text-overflow: clip;
}

.telemetry-grid .sub.bms-meta {
    margin-top: 6px;
    font-size: clamp(12px, 1.8vw, 15px);
    line-height: 1.18;
}

.telemetry-grid .value.bms-value {
    margin-top: 8px;
    margin-bottom: 0;
    position: relative;
    z-index: 2;
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
        margin-bottom: 6px;
        padding-top: 4px;
        padding-bottom: 6px;
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

    .telemetry-grid .sub.multiline {
        white-space: normal;
    }
}
)rawliteral";
