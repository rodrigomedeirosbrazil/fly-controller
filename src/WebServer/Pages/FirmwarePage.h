#pragma once

#include "CommonLayout.h"

inline String renderFirmwarePage() {
    const char* body = R"rawliteral(
<div class="panel">
    <h1>Atualização de Firmware</h1>
    <p>Selecione um arquivo <code>.bin</code> para atualizar o dispositivo.</p>
    <form id="fwForm">
        <input type="file" name="update" accept=".bin">
        <button type="submit">Atualizar Firmware</button>
    </form>
    <div id="response" class="message"></div>
</div>
)rawliteral";

    const char* script = R"rawliteral(
$('fwForm').addEventListener('submit', function(e) {
    e.preventDefault();
    const formData = new FormData(e.target);
    const responseDiv = $('response');
    responseDiv.textContent = 'Atualizando...';
    responseDiv.className = 'message';
    responseDiv.style.display = 'block';

    fetch('/update', { method: 'POST', body: formData })
        .then((r) => r.text())
        .then((text) => {
            responseDiv.textContent = text;
            responseDiv.classList.add(text.includes('Sucesso') || text.includes('andamento') ? 'ok' : 'err');
        })
        .catch((err) => {
            responseDiv.textContent = `Erro: ${err}`;
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
        nullptr,
        "page narrow",
        nullptr
    };

    return renderPage(spec);
}
