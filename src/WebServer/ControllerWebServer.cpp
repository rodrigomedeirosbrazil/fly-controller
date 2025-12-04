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
    <title>FlyController Firmware Update</title>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; text-align: center; margin: 50px; background-color: #f4f4f4; }
        .container { background-color: #fff; padding: 30px; border-radius: 8px; box-shadow: 0 0 10px rgba(0, 0, 0, 0.1); display: inline-block; }
        h1 { color: #333; }
        p { color: #666; margin-bottom: 20px; }
        form { margin-top: 20px; }
        input[type="file"] { border: 1px solid #ddd; padding: 10px; border-radius: 4px; }
        input[type="submit"] { background-color: #007bff; color: white; padding: 10px 20px; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; margin-left: 10px; }
        input[type="submit"]:hover { background-color: #0056b3; }
        .message { margin-top: 20px; padding: 10px; border-radius: 4px; }
        .success { background-color: #d4edda; color: #155724; border-color: #c3e6cb; }
        .error { background-color: #f8d7da; color: #721c24; border-color: #f5c6cb; }
    </style>
</head>
<body>
    <div class="container">
        <h1>FlyController</h1>
        <p>You are connected to the FlyController accelerator.</p>
        <p>Select a firmware file (.bin) to update the device.</p>
        <form method="POST" action="/update" enctype="multipart/form-data">
            <input type="file" name="update" accept=".bin">
            <input type="submit" value="Update">
        </form>
        <div id="response" class="message"></div>
    </div>
    <script>
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
                    setTimeout(() => {
                        window.location.reload(); // Reload after update and reboot
                    }, 5000); // Give time for reboot
                } else {
                    responseDiv.classList.add('error');
                }
            })
            .catch(error => {
                responseDiv.textContent = 'Error connecting to server: ' + error;
                responseDiv.classList.add('error');
            });
        });
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
