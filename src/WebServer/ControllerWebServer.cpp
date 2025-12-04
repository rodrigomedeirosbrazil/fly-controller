#include "ControllerWebServer.h"
#include <ESPmDNS.h>
#include "../config.h"
#include <Update.h>
#include <LittleFS.h>

const char* SOFT_AP_SSID = "FlyController";

IPAddress apIP(192, 168, 4, 1);
IPAddress netMsk(255, 255, 255, 0);

const char* INDEX_HTML = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>FlyController Manager</title>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; text-align: center; margin: 20px; background-color: #f4f4f4; }
        .container { background-color: #fff; padding: 20px; border-radius: 8px; box-shadow: 0 0 10px rgba(0, 0, 0, 0.1); max-width: 600px; margin: auto; }
        h1 { color: #333; }
        h2 { color: #555; border-bottom: 1px solid #eee; padding-bottom: 10px; }
        p { color: #666; margin-bottom: 20px; }
        form { margin-top: 20px; margin-bottom: 40px; }
        input[type="file"] { border: 1px solid #ddd; padding: 10px; border-radius: 4px; width: 100%; box-sizing: border-box; margin-bottom: 10px; }
        input[type="submit"] { background-color: #007bff; color: white; padding: 10px 20px; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; width: 100%; }
        input[type="submit"]:hover { background-color: #0056b3; }
        .message { margin-top: 20px; padding: 10px; border-radius: 4px; }
        .success { background-color: #d4edda; color: #155724; border-color: #c3e6cb; }
        .error { background-color: #f8d7da; color: #721c24; border-color: #f5c6cb; }
        
        table { width: 100%; border-collapse: collapse; margin-top: 20px; }
        th, td { text-align: left; padding: 12px; border-bottom: 1px solid #ddd; }
        th { background-color: #f8f9fa; }
        .btn-sm { padding: 5px 10px; font-size: 12px; margin: 2px; text-decoration: none; display: inline-block; border-radius: 3px; color: white; border: none; cursor: pointer;}
        .btn-download { background-color: #28a745; }
        .btn-delete { background-color: #dc3545; }
    </style>
</head>
<body>
    <div class="container">
        <h1>FlyController</h1>
        
        <h2>Firmware Update</h2>
        <p>Select a .bin file to update the device.</p>
        <form method="POST" action="/update" enctype="multipart/form-data">
            <input type="file" name="update" accept=".bin">
            <input type="submit" value="Update Firmware">
        </form>
        <div id="response" class="message"></div>

        <h2>Data Logs</h2>
        <p>Download or manage telemetry logs.</p>
        <table id="fileTable">
            <thead>
                <tr>
                    <th>File</th>
                    <th>Size</th>
                    <th>Action</th>
                </tr>
            </thead>
            <tbody>
                <tr><td colspan="3">Loading...</td></tr>
            </tbody>
        </table>
    </div>

    <script>
        // Firmware Update Logic
        document.querySelector('form').addEventListener('submit', function(e) {
            e.preventDefault();
            const form = e.target;
            const formData = new FormData(form);
            const responseDiv = document.getElementById('response');
            responseDiv.className = 'message';
            responseDiv.textContent = 'Updating...';

            fetch('/update', {
                method: 'POST',
                body: formData
            })
            .then(response => response.text())
            .then(data => {
                responseDiv.textContent = data;
                if (data.includes('Success')) {
                    responseDiv.classList.add('success');
                    setTimeout(() => { window.location.reload(); }, 5000);
                } else {
                    responseDiv.classList.add('error');
                }
            })
            .catch(error => {
                responseDiv.textContent = 'Error: ' + error;
                responseDiv.classList.add('error');
            });
        });

        // File Manager Logic
        function formatBytes(bytes, decimals = 2) {
            if (bytes === 0) return '0 Bytes';
            const k = 1024;
            const dm = decimals < 0 ? 0 : decimals;
            const sizes = ['Bytes', 'KB', 'MB', 'GB'];
            const i = Math.floor(Math.log(bytes) / Math.log(k));
            return parseFloat((bytes / Math.pow(k, i)).toFixed(dm)) + ' ' + sizes[i];
        }

        function loadFiles() {
            fetch('/list')
                .then(response => response.json())
                .then(files => {
                    const tbody = document.querySelector('#fileTable tbody');
                    tbody.innerHTML = '';
                    
                    if(files.length === 0) {
                        tbody.innerHTML = '<tr><td colspan="3">No logs found.</td></tr>';
                        return;
                    }

                    // Sort files (newest first)
                    files.sort((a, b) => b.name.localeCompare(a.name));

                    files.forEach(file => {
                        const tr = document.createElement('tr');
                        const downloadLink = '/logs' + file.name;
                        
                        tr.innerHTML = `
                            <td>${file.name.replace('/', '')}</td>
                            <td>${formatBytes(file.size)}</td>
                            <td>
                                <a href="${downloadLink}" class="btn-sm btn-download" download>Download</a>
                                <button onclick="deleteFile('${file.name}')" class="btn-sm btn-delete">Delete</button>
                            </td>
                        `;
                        tbody.appendChild(tr);
                    });
                })
                .catch(err => {
                    console.error(err);
                    document.querySelector('#fileTable tbody').innerHTML = '<tr><td colspan="3">Error loading files</td></tr>';
                });
        }

        function deleteFile(filename) {
            if(confirm('Are you sure you want to delete ' + filename + '?')) {
                fetch('/delete?file=' + filename)
                    .then(response => {
                        if(response.ok) {
                            loadFiles();
                        } else {
                            alert('Failed to delete file');
                        }
                    });
            }
        }

        // Load files on start
        loadFiles();
    </script>
</body>
</html>
)rawliteral";

ControllerWebServer::ControllerWebServer() : server(80) { // Initialize server on port 80
    isActive = true;
}

void ControllerWebServer::begin() {
    isActive = true;
    startAP();
}

void ControllerWebServer::startAP() {
    Serial.println("Configuring access point...");
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, netMsk);
    WiFi.softAP(SOFT_AP_SSID);

    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());

    dnsServer.start(53, "*", apIP);

    // Handle root URL
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", INDEX_HTML);
    });

    // Serve static files from LittleFS under /logs
    server.serveStatic("/logs", LittleFS, "/");

    // List files API
    server.on("/list", HTTP_GET, [](AsyncWebServerRequest *request){
        String json = "[";
        File root = LittleFS.open("/");
        if(root){
            File file = root.openNextFile();
            bool first = true;
            while(file){
                String fileName = String(file.name());
                // Ensure leading slash for consistency
                if(!fileName.startsWith("/")) fileName = "/" + fileName;
                
                // Only list .txt files (logs)
                if(fileName.endsWith(".txt")) {
                    if(!first) json += ",";
                    first = false;
                    json += "{\"name\":\"" + fileName + "\",\"size\":" + String(file.size()) + "}";
                }
                file = root.openNextFile();
            }
        }
        json += "]";
        request->send(200, "application/json", json);
    });

    // Delete file API
    server.on("/delete", HTTP_GET, [](AsyncWebServerRequest *request){
        if(request->hasParam("file")){
            String filename = request->getParam("file")->value();
            // Security: basic check to ensure we only delete what we expect
            if(!filename.startsWith("/")) filename = "/" + filename;
            
            if(LittleFS.exists(filename)){
                LittleFS.remove(filename);
                request->send(200, "text/plain", "Deleted");
            } else {
                request->send(404, "text/plain", "File not found");
            }
        } else {
            request->send(400, "text/plain", "Missing file param");
        }
    });

    // Handle firmware update POST request
    server.on(
        "/update",
        HTTP_POST,
        [](AsyncWebServerRequest *request) {
            // This is the 'final' callback for the POST request, after file upload is complete.
            request->send(200, "text/plain", (Update.hasError()) ? "Update failed!" : "Firmware update in progress. The device will reboot.");
            if (!Update.hasError()) {
                // If update was successful, ElegantOTA usually triggers reboot internally.
                // If not, we explicitly restart here.
                ESP.restart();
            }
        },
        [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
            // This is the file upload handler for chunks.
            if (!index) { // First chunk of the file
                Serial.printf("Update Start: %s\n", filename.c_str());
                if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { // Start with unknown size for file uploads
                    Update.printError(Serial);
                }
            }

            if (len) { // Write incoming data to the Update stream
                if (Update.write(data, len) != len) {
                    Update.printError(Serial);
                }
            }

            if (final) { // Last chunk of the file
                if (Update.end(true)) { // true to set a reboot flag
                    Serial.printf("Update Success: %uB\n", index + len);
                } else {
                    Update.printError(Serial);
                }
            }
        }
    );

    // Set up captive portal redirection for any request
    // ElegantOTA will handle its own routes, so we just need to redirect GET requests
    server.onNotFound([](AsyncWebServerRequest *request) {
        if (request->method() == HTTP_GET) {
            request->redirect("http://" + apIP.toString());
        } else {
            request->send(404);
        }
    });

    server.begin(); // Start the web server
    ElegantOTA.begin(&server); // Start ElegantOTA
    Serial.println("Web server started.");
}

void ControllerWebServer::stop() {
    Serial.println("Stopping web server and access point...");
    server.end(); // Stop the web server
    dnsServer.stop(); // Stop the DNS server
    WiFi.softAPdisconnect(true); // Disconnect clients and stop AP
    WiFi.mode(WIFI_OFF); // Turn off Wi-Fi
    isActive = false;
    Serial.println("Web server and access point stopped.");
}

void ControllerWebServer::handleClient() {
    // Only stop the web server if it's active AND the throttle is calibrated AND no update is in progress.
    if (isActive && throttle.isCalibrated() && !Update.isRunning()) {
        stop();
    }

    if (isActive) {
        ElegantOTA.loop(); // Process ElegantOTA events only if the server is active
    }

    // The DNSServer might need to be serviced even if the web server is stopped
    dnsServer.processNextRequest();
}
