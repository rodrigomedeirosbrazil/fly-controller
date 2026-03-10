#pragma once

const char* COMMON_JS = R"rawliteral(
const $ = (id) => document.getElementById(id);

const setText = (id, value) => {
    const el = $(id);
    if (!el) return;
    if (el.textContent !== value) {
        el.textContent = value;
    }
};

const fetchJson = (url) => fetch(url).then((r) => r.json());

const formatBytes = (bytes) => {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
};

const showMessage = (id, text, kind) => {
    const el = $(id);
    if (!el) return;
    el.textContent = text;
    el.className = `message ${kind}`;
    el.style.display = 'block';
};
)rawliteral";
