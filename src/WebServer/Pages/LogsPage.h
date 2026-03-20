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
</div>
)rawliteral";

    const char* script = R"rawliteral(
const loadFiles = () => {
    fetchJson('/list')
        .then((files) => {
            const tbody = document.querySelector('#fileTable tbody');
            tbody.innerHTML = '';
            if (files.length === 0) {
                tbody.innerHTML = '<tr><td colspan="3">No logs found.</td></tr>';
                return;
            }
            files.sort((a, b) => b.name.localeCompare(a.name));
            files.forEach((f) => {
                const tr = document.createElement('tr');
                tr.innerHTML = `
                    <td>${f.name.replace('/', '')}</td>
                    <td>${formatBytes(f.size)}</td>
                    <td>
                        <a class="btn btn-sm btn-green" href="/logs${f.name}" download>Download</a>
                        <button class="btn btn-sm btn-red" onclick="deleteFile('${f.name}')">Delete</button>
                    </td>`;
                tbody.appendChild(tr);
            });
        })
        .catch(() => {
            document.querySelector('#fileTable tbody').innerHTML = '<tr><td colspan="3">Error loading files.</td></tr>';
        });
};

const deleteFile = (filename) => {
    if (!confirm(`Delete ${filename}?`)) return;
    fetch('/delete?file=' + filename).then((r) => r.ok ? loadFiles() : alert('Delete failed'));
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
        nullptr
    };

    return renderPage(spec);
}
