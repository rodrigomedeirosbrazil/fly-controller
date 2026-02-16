#include "ControllerWebServer.h"
#include <ESPmDNS.h>
#include "../config.h"
#include "../Settings/Settings.h"
#include <Update.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

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
        .btn-config { background-color: #17a2b8; color: white; padding: 10px 20px; text-decoration: none; display: inline-block; border-radius: 4px; margin: 10px 0; font-size: 16px; }
        .btn-config:hover { background-color: #138496; }
    </style>
</head>
<body>
    <div class="container">
        <h1>FlyController</h1>

        <a href="/config" class="btn-config">⚙️ Configuration</a>

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

const char* CONFIG_HTML = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>FlyController - Configuration</title>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background-color: #f4f4f4; }
        .container { background-color: #fff; padding: 20px; border-radius: 8px; box-shadow: 0 0 10px rgba(0, 0, 0, 0.1); max-width: 600px; margin: auto; }
        h1 { color: #333; text-align: center; }
        h2 { color: #555; border-bottom: 1px solid #eee; padding-bottom: 10px; margin-top: 30px; }
        .form-group { margin-bottom: 20px; }
        label { display: block; margin-bottom: 5px; color: #666; font-weight: bold; }
        input[type="number"], select { width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; font-size: 14px; }
        select { cursor: pointer; }
        .info-text { color: #888; font-size: 12px; margin-top: 5px; }
        .total-voltage { background-color: #e7f3ff; padding: 8px; border-radius: 4px; margin-top: 5px; font-size: 12px; color: #0066cc; }
        button { background-color: #007bff; color: white; padding: 12px 24px; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; width: 100%; margin-top: 20px; }
        button:hover { background-color: #0056b3; }
        button:disabled { background-color: #ccc; cursor: not-allowed; }
        .message { margin-top: 20px; padding: 10px; border-radius: 4px; display: none; }
        .success { background-color: #d4edda; color: #155724; border: 1px solid #c3e6cb; }
        .error { background-color: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }
        .back-link { display: block; text-align: center; margin-top: 20px; color: #007bff; text-decoration: none; }
        .back-link:hover { text-decoration: underline; }
    </style>
</head>
<body>
    <div class="container">
        <h1>FlyController Configuration</h1>

        <form id="configForm">
            <h2>Battery Settings</h2>

            <div class="form-group">
                <label for="batteryCapacity">Battery Capacity (Ah):</label>
                <select id="batteryCapacity" name="batteryCapacity">
                    <option value="18">18 Ah</option>
                    <option value="35">35 Ah</option>
                    <option value="65">65 Ah</option>
                    <option value="custom">Custom (enter value)</option>
                </select>
                <input type="number" id="batteryCapacityCustom" name="batteryCapacityCustom" min="1" max="200" step="0.1" placeholder="Enter capacity in Ah" style="display: none; margin-top: 10px;">
                <div class="info-text">Select from preset values or choose custom to enter your own capacity.</div>
            </div>

            <div class="form-group">
                <label for="minVoltagePerCell">Minimum Voltage per Cell (V):</label>
                <input type="number" id="minVoltagePerCell" name="minVoltagePerCell" min="2.5" max="4.5" step="0.01" required>
                <div class="info-text">Minimum safe voltage per cell (typically 3.0V - 3.15V for LiPo).</div>
                <div class="total-voltage" id="minVoltageTotal">Total: <span id="minVoltageTotalValue">0.00</span> V (14 cells)</div>
            </div>

            <div class="form-group">
                <label for="maxVoltagePerCell">Maximum Voltage per Cell (V):</label>
                <input type="number" id="maxVoltagePerCell" name="maxVoltagePerCell" min="2.5" max="4.5" step="0.01" required>
                <div class="info-text">Maximum safe voltage per cell (typically 4.1V - 4.2V for LiPo).</div>
                <div class="total-voltage" id="maxVoltageTotal">Total: <span id="maxVoltageTotalValue">0.00</span> V (14 cells)</div>
            </div>

            <h2>Motor Temperature Settings</h2>

            <div class="form-group">
                <label for="motorMaxTemp">Maximum Motor Temperature (°C):</label>
                <input type="number" id="motorMaxTemp" name="motorMaxTemp" min="0" max="150" step="1" required>
                <div class="info-text">Motor will be completely disabled at this temperature.</div>
            </div>

            <div class="form-group">
                <label for="motorTempReductionStart">Motor Temperature Reduction Start (°C):</label>
                <input type="number" id="motorTempReductionStart" name="motorTempReductionStart" min="0" max="150" step="1" required>
                <div class="info-text">Power reduction begins at this temperature and increases linearly until maximum temperature.</div>
            </div>

            <h2>ESC Temperature Settings</h2>

            <div class="form-group">
                <label for="escMaxTemp">Maximum ESC Temperature (°C):</label>
                <input type="number" id="escMaxTemp" name="escMaxTemp" min="0" max="150" step="1" required>
                <div class="info-text">ESC will be completely disabled at this temperature.</div>
            </div>

            <div class="form-group">
                <label for="escTempReductionStart">ESC Temperature Reduction Start (°C):</label>
                <input type="number" id="escTempReductionStart" name="escTempReductionStart" min="0" max="150" step="1" required>
                <div class="info-text">Power reduction begins at this temperature and increases linearly until maximum temperature.</div>
            </div>

            <h2>Power Control Settings</h2>

            <div class="form-group">
                <label for="powerControlEnabled" style="display: flex; align-items: center; cursor: pointer;">
                    <input type="checkbox" id="powerControlEnabled" name="powerControlEnabled" style="width: auto; margin-right: 10px; cursor: pointer;">
                    <span>Enable Power Control</span>
                </label>
                <div class="info-text">When enabled, power output is limited based on battery voltage, motor temperature, and ESC temperature. When disabled, full power is available without limitations.</div>
            </div>

            <button type="submit" id="saveButton">Save Configuration</button>
            <div class="message" id="message"></div>
        </form>

        <a href="/" class="back-link">← Back to Main Page</a>
    </div>

    <script>
        const CELL_COUNT = 14; // BATTERY_CELL_COUNT

        // Load current values
        function loadCurrentValues() {
            fetch('/config/values')
                .then(response => response.json())
                .then(data => {
                    // Battery capacity
                    const capacityAh = data.batteryCapacity / 1000;
                    if (capacityAh == 18 || capacityAh == 35 || capacityAh == 65) {
                        document.getElementById('batteryCapacity').value = capacityAh;
                        document.getElementById('batteryCapacityCustom').style.display = 'none';
                    } else {
                        document.getElementById('batteryCapacity').value = 'custom';
                        document.getElementById('batteryCapacityCustom').value = capacityAh;
                        document.getElementById('batteryCapacityCustom').style.display = 'block';
                    }

                    // Battery voltages (convert from millivolts to volts per cell)
                    const minVoltagePerCell = (data.batteryMinVoltage / 1000) / CELL_COUNT;
                    const maxVoltagePerCell = (data.batteryMaxVoltage / 1000) / CELL_COUNT;
                    document.getElementById('minVoltagePerCell').value = minVoltagePerCell.toFixed(2);
                    document.getElementById('maxVoltagePerCell').value = maxVoltagePerCell.toFixed(2);
                    updateVoltageTotals();

                    // Motor temperatures (convert from millicelsius to celsius)
                    document.getElementById('motorMaxTemp').value = data.motorMaxTemp / 1000;
                    document.getElementById('motorTempReductionStart').value = data.motorTempReductionStart / 1000;

                    // ESC temperatures (convert from millicelsius to celsius)
                    document.getElementById('escMaxTemp').value = data.escMaxTemp / 1000;
                    document.getElementById('escTempReductionStart').value = data.escTempReductionStart / 1000;

                    // Power control enabled
                    document.getElementById('powerControlEnabled').checked = data.powerControlEnabled || false;
                })
                .catch(error => {
                    console.error('Error loading values:', error);
                    showMessage('Error loading current configuration', 'error');
                });
        }

        // Update voltage totals when per-cell values change
        function updateVoltageTotals() {
            const minVoltagePerCell = parseFloat(document.getElementById('minVoltagePerCell').value) || 0;
            const maxVoltagePerCell = parseFloat(document.getElementById('maxVoltagePerCell').value) || 0;

            const minVoltageTotal = minVoltagePerCell * CELL_COUNT;
            const maxVoltageTotal = maxVoltagePerCell * CELL_COUNT;

            document.getElementById('minVoltageTotalValue').textContent = minVoltageTotal.toFixed(2);
            document.getElementById('maxVoltageTotalValue').textContent = maxVoltageTotal.toFixed(2);
        }

        // Handle battery capacity select change
        document.getElementById('batteryCapacity').addEventListener('change', function() {
            if (this.value === 'custom') {
                document.getElementById('batteryCapacityCustom').style.display = 'block';
                document.getElementById('batteryCapacityCustom').focus();
            } else {
                document.getElementById('batteryCapacityCustom').style.display = 'none';
            }
        });

        // Update voltage totals on input
        document.getElementById('minVoltagePerCell').addEventListener('input', updateVoltageTotals);
        document.getElementById('maxVoltagePerCell').addEventListener('input', updateVoltageTotals);

        // Handle form submission
        document.getElementById('configForm').addEventListener('submit', function(e) {
            e.preventDefault();

            const saveButton = document.getElementById('saveButton');
            const messageDiv = document.getElementById('message');
            saveButton.disabled = true;
            messageDiv.style.display = 'none';

            // Get battery capacity
            let capacityAh;
            if (document.getElementById('batteryCapacity').value === 'custom') {
                capacityAh = parseFloat(document.getElementById('batteryCapacityCustom').value);
            } else {
                capacityAh = parseFloat(document.getElementById('batteryCapacity').value);
            }

            // Validate
            if (!capacityAh || capacityAh < 1 || capacityAh > 200) {
                showMessage('Battery capacity must be between 1 and 200 Ah', 'error');
                saveButton.disabled = false;
                return;
            }

            // Prepare data
            const minVoltagePerCell = parseFloat(document.getElementById('minVoltagePerCell').value);
            const maxVoltagePerCell = parseFloat(document.getElementById('maxVoltagePerCell').value);
            const minVoltageTotal = minVoltagePerCell * CELL_COUNT;
            const maxVoltageTotal = maxVoltagePerCell * CELL_COUNT;

            const data = {
                batteryCapacity: Math.round(capacityAh * 1000), // Convert to mAh
                batteryMinVoltage: Math.round(minVoltageTotal * 1000), // Convert to millivolts
                batteryMaxVoltage: Math.round(maxVoltageTotal * 1000), // Convert to millivolts
                motorMaxTemp: Math.round(parseFloat(document.getElementById('motorMaxTemp').value) * 1000), // Convert to millicelsius
                motorTempReductionStart: Math.round(parseFloat(document.getElementById('motorTempReductionStart').value) * 1000),
                escMaxTemp: Math.round(parseFloat(document.getElementById('escMaxTemp').value) * 1000),
                escTempReductionStart: Math.round(parseFloat(document.getElementById('escTempReductionStart').value) * 1000),
                powerControlEnabled: document.getElementById('powerControlEnabled').checked
            };

            // Send to server
            fetch('/config/save', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify(data)
            })
            .then(response => response.text())
            .then(text => {
                if (text.includes('Success') || response.ok) {
                    showMessage('Configuration saved successfully!', 'success');
                } else {
                    showMessage('Error saving configuration: ' + text, 'error');
                }
                saveButton.disabled = false;
            })
            .catch(error => {
                showMessage('Error saving configuration: ' + error, 'error');
                saveButton.disabled = false;
            });
        });

        function showMessage(text, type) {
            const messageDiv = document.getElementById('message');
            messageDiv.textContent = text;
            messageDiv.className = 'message ' + type;
            messageDiv.style.display = 'block';
        }

        // Load values on page load
        loadCurrentValues();
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

    // Get current configuration values (JSON) - register BEFORE /config to avoid conflicts
    server.on("/config/values", HTTP_GET, [](AsyncWebServerRequest *request){
        extern Settings settings;

        Serial.println("[WebServer] /config/values requested");

        DynamicJsonDocument doc(512);
        doc["batteryCapacity"] = settings.getBatteryCapacityMah();
        doc["batteryMinVoltage"] = settings.getBatteryMinVoltage();
        doc["batteryMaxVoltage"] = settings.getBatteryMaxVoltage();
        doc["motorMaxTemp"] = settings.getMotorMaxTemp();
        doc["motorTempReductionStart"] = settings.getMotorTempReductionStart();
        doc["escMaxTemp"] = settings.getEscMaxTemp();
        doc["escTempReductionStart"] = settings.getEscTempReductionStart();
        doc["powerControlEnabled"] = settings.getPowerControlEnabled();

        String response;
        serializeJson(doc, response);
        Serial.print("[WebServer] Sending JSON response: ");
        Serial.println(response);
        request->send(200, "application/json", response);
    });

    // Save configuration - register BEFORE /config to avoid conflicts
    server.on("/config/save", HTTP_POST, [](AsyncWebServerRequest *request){}, nullptr,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
            static String body;
            if (index == 0) {
                body = "";
            }
            body += String((char*)data);

            if (index + len == total) {
                extern Settings settings;

                DynamicJsonDocument doc(512);
                DeserializationError error = deserializeJson(doc, body);

                if (error) {
                    request->send(400, "text/plain", "Invalid JSON: " + String(error.c_str()));
                    return;
                }

                // Validate and set values
                if (doc.containsKey("batteryCapacity")) {
                    uint16_t capacity = doc["batteryCapacity"];
                    if (capacity >= 1000 && capacity <= 200000) {
                        settings.setBatteryCapacityMah(capacity);
                    } else {
                        request->send(400, "text/plain", "Battery capacity out of range (1000-200000 mAh)");
                        return;
                    }
                }

                if (doc.containsKey("batteryMinVoltage")) {
                    uint16_t minV = doc["batteryMinVoltage"];
                    if (minV >= 2500 && minV <= 63000) { // 2.5V to 63V (4.5V * 14 cells)
                        settings.setBatteryMinVoltage(minV);
                    } else {
                        request->send(400, "text/plain", "Battery min voltage out of range");
                        return;
                    }
                }

                if (doc.containsKey("batteryMaxVoltage")) {
                    uint16_t maxV = doc["batteryMaxVoltage"];
                    if (maxV >= 2500 && maxV <= 63000) {
                        settings.setBatteryMaxVoltage(maxV);
                    } else {
                        request->send(400, "text/plain", "Battery max voltage out of range");
                        return;
                    }
                }

                if (doc.containsKey("motorMaxTemp")) {
                    int32_t temp = doc["motorMaxTemp"];
                    if (temp >= 0 && temp <= 150000) {
                        settings.setMotorMaxTemp(temp);
                    } else {
                        request->send(400, "text/plain", "Motor max temp out of range (0-150000 millicelsius)");
                        return;
                    }
                }

                if (doc.containsKey("motorTempReductionStart")) {
                    int32_t temp = doc["motorTempReductionStart"];
                    if (temp >= 0 && temp <= 150000) {
                        settings.setMotorTempReductionStart(temp);
                    } else {
                        request->send(400, "text/plain", "Motor temp reduction start out of range");
                        return;
                    }
                }

                if (doc.containsKey("escMaxTemp")) {
                    int32_t temp = doc["escMaxTemp"];
                    if (temp >= 0 && temp <= 150000) {
                        settings.setEscMaxTemp(temp);
                    } else {
                        request->send(400, "text/plain", "ESC max temp out of range");
                        return;
                    }
                }

                if (doc.containsKey("escTempReductionStart")) {
                    int32_t temp = doc["escTempReductionStart"];
                    if (temp >= 0 && temp <= 150000) {
                        settings.setEscTempReductionStart(temp);
                    } else {
                        request->send(400, "text/plain", "ESC temp reduction start out of range");
                        return;
                    }
                }

                if (doc.containsKey("powerControlEnabled")) {
                    bool enabled = doc["powerControlEnabled"];
                    settings.setPowerControlEnabled(enabled);
                }

                // Save to Preferences
                settings.save();

                request->send(200, "text/plain", "Success: Configuration saved");
            }
        });

    // Configuration page - register AFTER /config/values and /config/save to avoid route conflicts
    server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", CONFIG_HTML);
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

    // Set up captive portal redirection for any request (BEFORE server.begin())
    // ElegantOTA will handle its own routes, so we just need to redirect GET requests
    server.onNotFound([](AsyncWebServerRequest *request) {
        // Don't redirect API endpoints or config page
        String url = request->url();
        if (url.startsWith("/config")) {
            // Let the specific /config routes handle it, or return 404 if not found
            request->send(404);
            return;
        }

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
        dnsServer.processNextRequest(); // Only process DNS when WiFi is active
    }
}
