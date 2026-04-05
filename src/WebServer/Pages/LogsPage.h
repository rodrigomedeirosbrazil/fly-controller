#pragma once

#include "CommonLayout.h"

inline String renderLogsPage() {
    const char* body = R"rawliteral(
<div class="panel">
    <h1>Data Logs</h1>
    <div class="table-wrap">
        <table id="fileTable">
            <thead><tr><th>File</th><th>Size</th><th>Action</th></tr></thead>
            <tbody><tr><td colspan="3">Loading...</td></tr></tbody>
        </table>
    </div>
    <div class="panel-footer" style="margin-top:1rem;">
        <button type="button" id="deleteAllBtn" class="btn btn-red" disabled onclick="deleteAllLogs()">Delete all logs</button>
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
                tbody.innerHTML = '<tr><td colspan="3">No logs found.</td></tr>';
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
                        <a class="btn btn-sm btn-green" href="/logs${f.name}" download="${downloadName}">Download CSV</a>
                        <button class="btn btn-sm btn-red" onclick="deleteFile('${f.name}')">Delete</button>
                    </td>`;
                tbody.appendChild(tr);
            });
        })
        .catch(() => {
            document.querySelector('#fileTable tbody').innerHTML = '<tr><td colspan="3">Error loading files.</td></tr>';
            const deleteAllBtn = document.querySelector('#deleteAllBtn');
            if (deleteAllBtn) deleteAllBtn.disabled = true;
        });
};

const deleteFile = (filename) => {
    if (!confirm(`Delete ${filename}?`)) return;
    fetch('/delete?file=' + filename).then((r) => r.ok ? loadFiles() : alert('Delete failed'));
};

const deleteAllLogs = () => {
    const deleteAllBtn = document.querySelector('#deleteAllBtn');
    if (deleteAllBtn && deleteAllBtn.disabled) return;
    if (!confirm('This will permanently delete all log files on the device. Continue?')) return;
    fetch('/delete-all-logs').then((r) => (r.ok ? loadFiles() : alert('Delete all failed')));
};

loadFiles();
)rawliteral";

    PageSpec spec = {
        "FlyController - Logs",
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
