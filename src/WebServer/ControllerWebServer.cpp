#include "ControllerWebServer.h"
#include <ESPmDNS.h>
#include "../config.h"
#include "../Settings/Settings.h"
#include "../BoardConfig.h"
#include <Update.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "Pages/CommonLayout.h"
#include "Pages/ConfigPage.h"
#include "Pages/DashboardPage.h"
#include "Pages/FirmwarePage.h"
#include "Pages/LogsPage.h"
#include "Pages/TelemetryPage.h"
#include "Pages/LegacyIndexPage.h"
#include "../Version.h"

const char* SOFT_AP_SSID = "FlyController";

IPAddress apIP(192, 168, 4, 1);
IPAddress netMsk(255, 255, 255, 0);

#if IS_HOBBYWING
const char* CONTROLLER_LABEL = "Hobbywing";
#elif IS_TMOTOR
const char* CONTROLLER_LABEL = "Tmotor";
#elif IS_XAG
const char* CONTROLLER_LABEL = "XAG";
#else
const char* CONTROLLER_LABEL = "Unknown";
#endif


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
        String page = renderDashboardPage();
        applyCommonTokens(page, APP_VERSION, __DATE__, __TIME__, CONTROLLER_LABEL);
        request->send(200, "text/html", page);
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
        doc["wifiAutoDisableAfterCalibration"] = settings.getWifiAutoDisableAfterCalibration();

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

                if (doc.containsKey("wifiAutoDisableAfterCalibration")) {
                    bool enabled = doc["wifiAutoDisableAfterCalibration"];
                    settings.setWifiAutoDisableAfterCalibration(enabled);
                }

                // Save to Preferences
                settings.save();

                request->send(200, "text/plain", "Success: Configuration saved");
            }
        });

    // Telemetry API
    server.on("/api/telemetry", HTTP_GET, [](AsyncWebServerRequest *request){
        DynamicJsonDocument doc(512);

        const bool hasTelemetry = telemetry.hasData();
        const bool hasCurrentSensor = getBoardConfig().hasCurrentSensor;
        const uint16_t batteryVoltageMv = telemetry.getBatteryVoltageMilliVolts();
        const uint32_t batteryCurrentMa = telemetry.getBatteryCurrentMilliAmps();

        // kW x10 to avoid float over JSON transport (7 => 0.7 kW)
        uint16_t powerKwX10 = 0;
        if (hasTelemetry && hasCurrentSensor) {
            const uint32_t powerMilliWatts = ((uint32_t) batteryVoltageMv * batteryCurrentMa) / 1000;
            powerKwX10 = (uint16_t) (powerMilliWatts / 100000);
        }

        doc["hasTelemetry"] = hasTelemetry;
        doc["batteryPercentCc"] = batteryMonitor.getSoC();
        doc["batteryPercentVoltage"] = batteryMonitor.getSoCFromVoltage();
        doc["batteryVoltageMv"] = batteryVoltageMv;
        doc["powerKwX10"] = powerKwX10;
        doc["throttlePercent"] = throttle.getThrottlePercentage();
        doc["throttleRaw"] = throttle.getThrottleRaw();
        doc["powerPercent"] = power.getPower();
        doc["motorTempMc"] = telemetry.getMotorTempMilliCelsius();
        doc["rpm"] = hasCurrentSensor ? telemetry.getRpm() : 0;
        doc["escCurrentMa"] = hasCurrentSensor ? batteryCurrentMa : 0;
        doc["escTempMc"] = telemetry.getEscTempMilliCelsius();
        doc["armed"] = throttle.isArmed();
        doc["uptimeMs"] = millis();
        doc["lastTelemetryUpdateMs"] = telemetry.getLastUpdate();

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // Configuration page - register AFTER /config/values and /config/save to avoid route conflicts
    server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", renderConfigPage());
    });

    server.on("/telemetry", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", renderTelemetryPage());
    });

    server.on("/firmware", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", renderFirmwarePage());
    });

    server.on("/logs-page", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", renderLogsPage());
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
    extern Settings settings;

    // Only stop the web server if auto-disable is enabled AND throttle is calibrated AND no update is in progress.
    if (
        isActive
        && settings.getWifiAutoDisableAfterCalibration()
        && throttle.isCalibrated()
        && !Update.isRunning()
    ) {
        stop();
    }

    if (isActive) {
        ElegantOTA.loop(); // Process ElegantOTA events only if the server is active
        dnsServer.processNextRequest(); // Only process DNS when WiFi is active
    }
}
