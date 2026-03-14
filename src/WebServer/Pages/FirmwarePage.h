#pragma once

#include "CommonLayout.h"

inline String renderFirmwarePage() {
    const char* body = R"rawliteral(
<div class="panel">
    <h1>Firmware Update</h1>
    <p>Select a <code>.bin</code> file to update the device.</p>
    <form id="fwForm">
        <input type="file" name="update" accept=".bin">
        <button type="submit">Update Firmware</button>
    </form>
    <div id="response" class="message"></div>
</div>
)rawliteral";

    const char* script = R"rawliteral(
$('fwForm').addEventListener('submit', function(e) {
    e.preventDefault();
    const formData = new FormData(e.target);
    const responseDiv = $('response');
    responseDiv.textContent = 'Updating...';
    responseDiv.className = 'message';
    responseDiv.style.display = 'block';

    fetch('/update', { method: 'POST', body: formData })
        .then((r) => r.text())
        .then((text) => {
            responseDiv.textContent = text;
            responseDiv.classList.add(text.includes('Success') || text.includes('progress') ? 'ok' : 'err');
        })
        .catch((err) => {
            responseDiv.textContent = `Error: ${err}`;
            responseDiv.classList.add('err');
        });
});
)rawliteral";

    PageSpec spec = {
        "FlyController - Firmware",
        "/firmware",
        body,
        nullptr,
        script,
        "page narrow"
    };

    return renderPage(spec);
}
