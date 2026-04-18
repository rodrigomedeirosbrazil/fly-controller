#pragma once

#include "CommonLayout.h"

inline String renderLogsPage() {
    const char* body = R"rawliteral(
<div class="panel">
    <h1>Registros de Dados</h1>
    <div class="form-group" style="max-width:320px;margin-bottom:1rem;">
        <label for="logPin">PIN</label>
        <input type="password" id="logPin" maxlength="8" placeholder="Necessário para excluir" oninput="setPin(this.value)">
    </div>
    <div class="table-wrap">
        <table id="fileTable">
            <thead><tr><th>Arquivo</th><th>Tamanho</th><th>Ação</th></tr></thead>
            <tbody><tr><td colspan="3">Carregando...</td></tr></tbody>
        </table>
    </div>
    <div class="panel-footer" style="margin-top:1rem;">
        <button type="button" id="deleteAllBtn" class="btn btn-red" disabled onclick="deleteAllLogs()">Excluir todos os registros</button>
    </div>
</div>
)rawliteral";

    const char* script = R"rawliteral(
const toCsvName = (filename) => filename.replace(/\.txt$/i, '.csv');

const loadFiles = () => {
    fetchJson('/list')
        .then((files) => {
            const tbody = document.querySelector('#fileTable tbody');
            const deleteAllBtn = document.querySelector('#deleteAllBtn');
            tbody.innerHTML = '';
            if (files.length === 0) {
                tbody.innerHTML = '<tr><td colspan="3">Nenhum registro encontrado.</td></tr>';
                if (deleteAllBtn) deleteAllBtn.disabled = true;
                return;
            }
            if (deleteAllBtn) deleteAllBtn.disabled = false;
            files.sort((a, b) => b.name.localeCompare(a.name));
            files.forEach((f) => {
                const displayName = toCsvName(f.name.replace('/', ''));
                const downloadName = toCsvName(f.name.replace(/^\//, ''));
                const tr = document.createElement('tr');
                tr.innerHTML = `
                    <td>${displayName}</td>
                    <td>${formatBytes(f.size)}</td>
                    <td>
                        <a class="btn btn-sm btn-green" href="/logs${f.name}" download="${downloadName}">Baixar CSV</a>
                        <button class="btn btn-sm btn-red" onclick="deleteFile('${f.name}')">Excluir</button>
                    </td>`;
                tbody.appendChild(tr);
            });
        })
        .catch(() => {
            document.querySelector('#fileTable tbody').innerHTML = '<tr><td colspan="3">Erro ao carregar arquivos.</td></tr>';
            const deleteAllBtn = document.querySelector('#deleteAllBtn');
            if (deleteAllBtn) deleteAllBtn.disabled = true;
        });
};

const deleteFile = (filename) => {
    if (!confirm(`Excluir ${filename}?`)) return;
    fetchWithPin('/delete?file=' + encodeURIComponent(filename))
        .then((r) => r.ok ? loadFiles() : r.text().then((t) => alert('Falha ao excluir: ' + t)));
};

const deleteAllLogs = () => {
    const deleteAllBtn = document.querySelector('#deleteAllBtn');
    if (deleteAllBtn && deleteAllBtn.disabled) return;
    if (!confirm('Isso excluirá permanentemente todos os arquivos de registro do dispositivo. Continuar?')) return;
    fetchWithPin('/delete-all-logs')
        .then((r) => r.ok ? loadFiles() : r.text().then((t) => alert('Falha ao excluir: ' + t)));
};

// Pre-fill PIN from sessionStorage on page load
window.addEventListener('DOMContentLoaded', () => {
    const pinInput = document.querySelector('#logPin');
    if (pinInput) pinInput.value = getPin();
});

loadFiles();
)rawliteral";

    PageSpec spec = {
        "FlyController - Registros",
        "/logs-page",
        body,
        nullptr,
        script,
        nullptr,
        nullptr,
        nullptr
    };

    return renderPage(spec);
}
