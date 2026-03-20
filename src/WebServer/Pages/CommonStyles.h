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
    display: inline-block;
    padding: 4px 10px;
    border-radius: 999px;
    font-size: 12px;
    font-weight: bold;
}

.status.live { background: #dcfce7; color: #166534; }
.status.stale { background: #fef9c3; color: #854d0e; }
.status.nodata { background: #fee2e2; color: #991b1b; }

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
        padding: 8px 10px 10px;
        display: flex;
        flex-direction: column;
        box-sizing: border-box;
    }

    body.telemetry-page .page.page-telemetry .topbar {
        flex-shrink: 0;
        margin-bottom: 8px;
        padding-top: 6px;
        padding-bottom: 8px;
    }

    body.telemetry-page .page.page-telemetry .topbar .nav-btn {
        padding: 8px 10px;
        font-size: 12px;
    }

    .telemetry-shell {
        flex: 1;
        min-height: 0;
        display: flex;
        flex-direction: column;
        gap: 8px;
        overflow: hidden;
    }

    .telemetry-header.panel {
        flex-shrink: 0;
        margin-bottom: 0;
        padding: 10px 12px;
    }

    .telemetry-header.panel h1 {
        font-size: clamp(16px, 4vw, 20px);
        margin-bottom: 4px;
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
        padding: 8px 10px;
        display: flex;
        flex-direction: column;
        justify-content: center;
        overflow: hidden;
    }

    .telemetry-grid .label {
        font-size: 10px;
    }

    .telemetry-grid .value {
        font-size: clamp(14px, 3.5vmin, 18px);
        white-space: nowrap;
        text-overflow: ellipsis;
        overflow: hidden;
    }

    .telemetry-grid .sub {
        font-size: clamp(10px, 2.8vmin, 12px);
        white-space: nowrap;
        text-overflow: ellipsis;
        overflow: hidden;
    }
}
)rawliteral";
