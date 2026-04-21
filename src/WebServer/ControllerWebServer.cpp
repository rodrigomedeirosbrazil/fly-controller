#include "ControllerWebServer.h"
#include <ESPmDNS.h>
#include "../config.h"
#include "../Settings/Settings.h"
#include "../BatteryMonitor/BatteryMonitor.h"
#include "../BoardConfig.h"
#include "../Telemetry/TelemetryAvailability.h"
#include <Update.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <esp_heap_caps.h>
#include <sys/time.h>
#include "Pages/CommonLayout.h"
#include "Pages/ConfigPowerPage.h"
#include "Pages/ConfigThermalPage.h"
#include "Pages/ConfigBmsPage.h"
#include "Pages/ConfigHubPage.h"
#include "Pages/DashboardPage.h"
#include "Pages/FirmwarePage.h"
#include "Pages/LogsPage.h"
#include "Pages/TelemetryPage.h"
#include "Pages/LegacyIndexPage.h"
#include "../Version.h"
#include "../Logger/Logger.h"
#include <vector>

extern Logger logger;

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

namespace {
void logWebHeap(const char* tag) {
    Serial.printf(
        "[WebServer] %s heap=%u min=%u largest=%u\n",
        tag,
        (unsigned)ESP.getFreeHeap(),
        (unsigned)ESP.getMinFreeHeap(),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)
    );
}

template <typename TJson>
void sendJsonResponse(AsyncWebServerRequest* request, int httpStatus, const TJson& json) {
    AsyncResponseStream* response = request->beginResponseStream("application/json");
    if (response == nullptr) {
        request->send(500, "text/plain", "Sem memória");
        return;
    }
    response->setCode(httpStatus);
    serializeJson(json, *response);
    request->send(response);
}

const char* toBmsScanStatusLabel(uint8_t status) {
    switch (status) {
        case BluetoothBmsScanScanning:
            return "scanning";
        case BluetoothBmsScanComplete:
            return "complete";
        case BluetoothBmsScanError:
            return "error";
        case BluetoothBmsScanIdle:
        default:
            return "idle";
    }
}

void sendBmsScanStatusResponse(AsyncWebServerRequest* request, int httpStatus = 200) {
    StaticJsonDocument<2048> doc;
    doc["ok"] = (httpStatus >= 200 && httpStatus < 300);
    doc["status"] = toBmsScanStatusLabel(bluetoothBms.getWebScanStatus());
    doc["busy"] = bluetoothBms.isWebScanBusy();

    if (bluetoothBms.getWebScanStatus() == BluetoothBmsScanError && strlen(bluetoothBms.getWebScanError()) > 0) {
        doc["error"] = bluetoothBms.getWebScanError();
    }

    JsonArray results = doc.createNestedArray("results");
    const BluetoothBmsScanResult* scanResults = bluetoothBms.getWebScanResults();
    const uint8_t resultCount = bluetoothBms.getWebScanResultCount();

    for (uint8_t i = 0; i < resultCount; i++) {
        JsonObject entry = results.createNestedObject();
        entry["mac"] = scanResults[i].mac;
        entry["name"] = scanResults[i].name;
        entry["rssi"] = scanResults[i].rssi;
        entry["detectedType"] = scanResults[i].detectedType;
        entry["advertisedServices"] = scanResults[i].advertisedServices;
    }

    if (doc.overflowed()) {
        request->send(500, "text/plain", "Estouro do buffer JSON");
        return;
    }

    sendJsonResponse(request, httpStatus, doc);
}

void sendPowerConfigResponse(AsyncWebServerRequest* request) {
    StaticJsonDocument<512> doc;
    doc["batteryCapacity"] = settings.getBatteryCapacityMah();
    doc["batteryMinVoltage"] = settings.getBatteryMinVoltage();
    doc["batteryMaxVoltage"] = settings.getBatteryMaxVoltage();
    doc["powerControlEnabled"] = settings.getPowerControlEnabled();
    doc["throttleCurveGamma"] = settings.getThrottleCurveGamma();
    if (doc.overflowed()) {
        request->send(500, "text/plain", "Estouro do buffer JSON");
        return;
    }
    sendJsonResponse(request, 200, doc);
}

void sendThermalConfigResponse(AsyncWebServerRequest* request) {
    StaticJsonDocument<512> doc;
    doc["motorMaxTemp"] = settings.getMotorMaxTemp();
    doc["motorTempReductionStart"] = settings.getMotorTempReductionStart();
    doc["escMaxTemp"] = settings.getEscMaxTemp();
    doc["escTempReductionStart"] = settings.getEscTempReductionStart();
    if (doc.overflowed()) {
        request->send(500, "text/plain", "Estouro do buffer JSON");
        return;
    }
    sendJsonResponse(request, 200, doc);
}

void sendBmsConfigResponse(AsyncWebServerRequest* request) {
    StaticJsonDocument<256> doc;
    doc["bmsType"] = settings.getBmsType();
    doc["bmsMac"] = settings.getBmsMac();
    if (doc.overflowed()) {
        request->send(500, "text/plain", "Estouro do buffer JSON");
        return;
    }
    sendJsonResponse(request, 200, doc);
}

// Returns true when the request carries the correct X-Config-Pin header.
// All write endpoints (config save, delete, OTA) must call this first.
bool checkPin(AsyncWebServerRequest* request) {
    if (!request->hasHeader("X-Config-Pin")) return false;
    return request->getHeader("X-Config-Pin")->value() == settings.getConfigPin();
}
} // namespace


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
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
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

    server.on("/api/config/power", HTTP_GET, [](AsyncWebServerRequest *request){
        logWebHeap("/api/config/power GET");
        sendPowerConfigResponse(request);
    });

    server.on("/api/config/thermal", HTTP_GET, [](AsyncWebServerRequest *request){
        logWebHeap("/api/config/thermal GET");
        sendThermalConfigResponse(request);
    });

    server.on("/api/config/bms", HTTP_GET, [](AsyncWebServerRequest *request){
        logWebHeap("/api/config/bms GET");
        sendBmsConfigResponse(request);
    });

    server.on("/api/settime", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        nullptr,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            if (index + len < total) return; // wait for full body
            char buf[24] = {0};
            size_t copyLen = len < sizeof(buf) - 1 ? len : sizeof(buf) - 1;
            memcpy(buf, data, copyLen);
            const int64_t epochMs = atoll(buf);
            if (epochMs > 1577836800000LL) { // sanity: > 2020-01-01
                struct timeval tv;
                tv.tv_sec  = (time_t)(epochMs / 1000);
                tv.tv_usec = (suseconds_t)((epochMs % 1000) * 1000);
                settimeofday(&tv, nullptr);
                request->send(200, "text/plain", "OK");
            } else {
                request->send(400, "text/plain", "Epoch inválido");
            }
        }
    );

    server.on("/api/bms/scan/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        logWebHeap("/api/bms/scan/status");
        sendBmsScanStatusResponse(request);
    });

    server.on("/api/bms/scan/start", HTTP_POST, [](AsyncWebServerRequest *request) {
        logWebHeap("/api/bms/scan/start");
        if (!checkPin(request)) { request->send(403, "text/plain", "PIN inválido"); return; }
        const bool started = bluetoothBms.startWebScan();
        const int httpStatus = started ? 200 : 500;
        sendBmsScanStatusResponse(request, httpStatus);
    });

    AsyncCallbackJsonWebHandler* detectHandler = new AsyncCallbackJsonWebHandler(
        "/api/bms/detect",
        [](AsyncWebServerRequest *request, JsonVariant &json) {
            logWebHeap("/api/bms/detect");
            if (!json.is<JsonObject>()) {
                request->send(400, "application/json", "{\"ok\":false,\"error\":\"JSON inválido\"}");
                return;
            }

            JsonObject doc = json.as<JsonObject>();
            const String mac = doc["mac"] | "";
            const uint8_t detectedType = bluetoothBms.detectBmsTypeByMac(mac);

            StaticJsonDocument<128> responseDoc;
            responseDoc["ok"] = true;
            responseDoc["mac"] = mac;
            responseDoc["detectedType"] = detectedType;
            if (responseDoc.overflowed()) {
                request->send(500, "text/plain", "Estouro do buffer JSON");
                return;
            }
            sendJsonResponse(request, 200, responseDoc);
        },
        256
    );
    detectHandler->setMethod(HTTP_POST);
    detectHandler->setMaxContentLength(256);
    server.addHandler(detectHandler);

    AsyncCallbackJsonWebHandler* savePowerHandler = new AsyncCallbackJsonWebHandler(
        "/api/config/power",
        [](AsyncWebServerRequest *request, JsonVariant &json) {
            logWebHeap("/api/config/power POST");
            if (!checkPin(request)) { request->send(403, "text/plain", "PIN inválido"); return; }
            if (!json.is<JsonObject>()) {
                request->send(400, "text/plain", "JSON inválido: o corpo deve ser um objeto");
                return;
            }

            JsonObject doc = json.as<JsonObject>();

            if (!doc.containsKey("batteryCapacity")
                || !doc.containsKey("batteryMinVoltage")
                || !doc.containsKey("batteryMaxVoltage")
                || !doc.containsKey("powerControlEnabled")
                || !doc.containsKey("throttleCurveGamma")) {
                request->send(400, "text/plain", "Campos obrigatórios de configuração de energia ausentes");
                return;
            }

            // Read as uint32_t first so the upper-bound check is meaningful —
            // uint16_t max is 65535, so "> 65000" would otherwise always be false.
            uint32_t capacityRaw = doc["batteryCapacity"].as<uint32_t>();
            if (capacityRaw < 1000 || capacityRaw > 65000) {
                request->send(400, "text/plain", "Capacidade da bateria fora do intervalo (1000-65000 mAh)");
                return;
            }
            uint16_t capacity = static_cast<uint16_t>(capacityRaw);

            uint16_t minV = doc["batteryMinVoltage"];
            if (minV < 2500 || minV > 63000) {
                request->send(400, "text/plain", "Tensão mínima da bateria fora do intervalo");
                return;
            }

            uint16_t maxV = doc["batteryMaxVoltage"];
            if (maxV < 2500 || maxV > 63000) {
                request->send(400, "text/plain", "Tensão máxima da bateria fora do intervalo");
                return;
            }

            float gamma = doc["throttleCurveGamma"].as<float>();
            if (gamma < 1.0f || gamma > 3.0f) {
                request->send(400, "text/plain", "O gamma da curva do acelerador deve ser entre 1,0 e 3,0");
                return;
            }

            settings.setBatteryCapacityMah(capacity);
            settings.setBatteryMinVoltage(minV);
            settings.setBatteryMaxVoltage(maxV);
            settings.setPowerControlEnabled(doc["powerControlEnabled"].as<bool>());
            settings.setThrottleCurveGamma(gamma);
            settings.save();
            // Keep the in-memory BatteryMonitor in sync with the new capacity
            // setting — without this, Coulomb counting uses the old value until reboot.
            batteryMonitor.setCapacity(settings.getBatteryCapacityMah());

            request->send(200, "text/plain", "Sucesso: Configurações de energia salvas");
        },
        512
    );
    savePowerHandler->setMethod(HTTP_POST);
    savePowerHandler->setMaxContentLength(512);
    server.addHandler(savePowerHandler);

    AsyncCallbackJsonWebHandler* saveThermalHandler = new AsyncCallbackJsonWebHandler(
        "/api/config/thermal",
        [](AsyncWebServerRequest *request, JsonVariant &json) {
            logWebHeap("/api/config/thermal POST");
            if (!checkPin(request)) { request->send(403, "text/plain", "PIN inválido"); return; }
            if (!json.is<JsonObject>()) {
                request->send(400, "text/plain", "JSON inválido: o corpo deve ser um objeto");
                return;
            }

            JsonObject doc = json.as<JsonObject>();

            if (!doc.containsKey("motorMaxTemp")
                || !doc.containsKey("motorTempReductionStart")
                || !doc.containsKey("escMaxTemp")
                || !doc.containsKey("escTempReductionStart")) {
                request->send(400, "text/plain", "Campos obrigatórios de configuração térmica ausentes");
                return;
            }

            int32_t motorMaxTemp = doc["motorMaxTemp"];
            int32_t motorReductionStart = doc["motorTempReductionStart"];
            int32_t escMaxTemp = doc["escMaxTemp"];
            int32_t escReductionStart = doc["escTempReductionStart"];

            if (motorMaxTemp < 0 || motorMaxTemp > 150000) {
                request->send(400, "text/plain", "Temperatura máxima do motor fora do intervalo (0-150000 millicelsius)");
                return;
            }

            if (motorReductionStart < 0 || motorReductionStart > 150000) {
                request->send(400, "text/plain", "Início de redução de temperatura do motor fora do intervalo");
                return;
            }

            if (escMaxTemp < 0 || escMaxTemp > 150000) {
                request->send(400, "text/plain", "Temperatura máxima do ESC fora do intervalo");
                return;
            }

            if (escReductionStart < 0 || escReductionStart > 150000) {
                request->send(400, "text/plain", "Início de redução de temperatura do ESC fora do intervalo");
                return;
            }

            settings.setMotorMaxTemp(motorMaxTemp);
            settings.setMotorTempReductionStart(motorReductionStart);
            settings.setEscMaxTemp(escMaxTemp);
            settings.setEscTempReductionStart(escReductionStart);
            settings.save();

            request->send(200, "text/plain", "Sucesso: Configurações térmicas salvas");
        },
        512
    );
    saveThermalHandler->setMethod(HTTP_POST);
    saveThermalHandler->setMaxContentLength(512);
    server.addHandler(saveThermalHandler);

    AsyncCallbackJsonWebHandler* saveBmsHandler = new AsyncCallbackJsonWebHandler(
        "/api/config/bms",
        [](AsyncWebServerRequest *request, JsonVariant &json) {
            logWebHeap("/api/config/bms POST");
            if (!checkPin(request)) { request->send(403, "text/plain", "PIN inválido"); return; }
            if (!json.is<JsonObject>()) {
                request->send(400, "text/plain", "JSON inválido: o corpo deve ser um objeto");
                return;
            }

            JsonObject doc = json.as<JsonObject>();
            if (!doc.containsKey("bmsType") || !doc.containsKey("bmsMac")) {
                request->send(400, "text/plain", "Campos obrigatórios de configuração do BMS ausentes");
                return;
            }

            const uint8_t bmsType = doc["bmsType"].as<uint8_t>();
            if (bmsType > BmsTypeDaly) {
                request->send(400, "text/plain", "Tipo de BMS inválido");
                return;
            }

            String s = doc["bmsMac"].as<const char*>() ? doc["bmsMac"].as<const char*>() : "";
            s.trim();

            if (s.length() > 0) {
                if (s.length() != 17) {
                    request->send(400, "text/plain", "O MAC do BMS deve ter 17 caracteres (XX:XX:XX:XX:XX:XX)");
                    return;
                }
                for (int i = 0; i < 17; i++) {
                    if (i == 2 || i == 5 || i == 8 || i == 11 || i == 14) {
                        if (s[i] != ':') {
                            request->send(400, "text/plain", "Formato do MAC do BMS: XX:XX:XX:XX:XX:XX");
                            return;
                        }
                    } else {
                        const char c = s[i];
                        const bool hex = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
                        if (!hex) {
                            request->send(400, "text/plain", "O MAC do BMS deve usar dígitos hexadecimais (0-9, A-F)");
                            return;
                        }
                    }
                }
            }

            if (bmsType != BmsTypeNone && s.length() != 17) {
                request->send(400, "text/plain", "O MAC do BMS deve ser configurado para o tipo de BMS selecionado");
                return;
            }

            settings.setBmsType(bmsType);
            settings.setBmsMac(s.c_str());
            settings.save();

            request->send(200, "text/plain", "Sucesso: Configurações do BMS salvas");
        },
        256
    );
    saveBmsHandler->setMethod(HTTP_POST);
    saveBmsHandler->setMaxContentLength(256);
    server.addHandler(saveBmsHandler);

    // PIN management — GET: check if pin is "0000" (default); POST: change pin.
    // The POST body must be { "currentPin": "xxxx", "newPin": "yyyy" }.
    // newPin must be 4-8 characters. Current PIN is validated before applying.
    server.on("/api/config/pin", HTTP_GET, [](AsyncWebServerRequest *request) {
        StaticJsonDocument<64> doc;
        doc["isDefault"] = (settings.getConfigPin() == "0000");
        if (doc.overflowed()) {
            request->send(500, "text/plain", "Estouro do buffer JSON");
            return;
        }
        sendJsonResponse(request, 200, doc);
    });

    AsyncCallbackJsonWebHandler* savePinHandler = new AsyncCallbackJsonWebHandler(
        "/api/config/pin",
        [](AsyncWebServerRequest *request, JsonVariant &json) {
            if (!json.is<JsonObject>()) {
                request->send(400, "text/plain", "JSON inválido");
                return;
            }
            JsonObject doc = json.as<JsonObject>();
            if (!doc.containsKey("currentPin") || !doc.containsKey("newPin")) {
                request->send(400, "text/plain", "currentPin ou newPin ausente");
                return;
            }
            const String currentPin = doc["currentPin"].as<const char*>();
            const String newPin     = doc["newPin"].as<const char*>();
            if (currentPin != settings.getConfigPin()) {
                request->send(403, "text/plain", "O PIN atual está incorreto");
                return;
            }
            if (newPin.length() < 4 || newPin.length() > 8) {
                request->send(400, "text/plain", "O novo PIN deve ter 4 a 8 caracteres");
                return;
            }
            settings.setConfigPin(newPin);
            settings.save();
            ElegantOTA.setAuth("admin", settings.getConfigPin().c_str());
            request->send(200, "text/plain", "PIN atualizado com sucesso");
        },
        128
    );
    savePinHandler->setMethod(HTTP_POST);
    savePinHandler->setMaxContentLength(128);
    server.addHandler(savePinHandler);

    // Telemetry API
    server.on("/api/telemetry", HTTP_GET, [](AsyncWebServerRequest *request){
        StaticJsonDocument<768> doc;

        const bool hasTelemetry = telemetry.hasData();
        const uint16_t batteryVoltageMv = telemetry.getBatteryVoltageMilliVolts();
        const uint32_t batteryCurrentMa = telemetry.getBatteryCurrentMilliAmps();

        // kW x10 to avoid float over JSON transport (7 => 0.7 kW)
        uint16_t powerKwX10 = 0;
        if (isPowerKwAvailable()) {
            const uint32_t powerMilliWatts = ((uint32_t) batteryVoltageMv * batteryCurrentMa) / 1000;
            powerKwX10 = (uint16_t) (powerMilliWatts / 100000);
        }

        // Availability: explicit flags so frontend can show N/A vs 0
        JsonObject availability = doc.createNestedObject("availability");
        availability["current"] = isCurrentAvailable();
        availability["rpm"] = isRpmAvailable();
        availability["powerKw"] = isPowerKwAvailable();
        availability["bms"] = isBmsDataAvailable();
        availability["bmsCells"] = isBmsCellDataAvailable();

        doc["hasTelemetry"] = hasTelemetry;
        doc["batteryPercentCc"] = batteryMonitor.getSoC();
        doc["batteryPercentVoltage"] = batteryMonitor.getSoCFromVoltage();
        doc["batteryVoltageMv"] = batteryVoltageMv;

        if (isPowerKwAvailable()) {
            doc["powerKwX10"] = powerKwX10;
        }
        doc["throttlePercent"] = throttle.getThrottlePercentage();
        doc["throttleRaw"] = throttle.getThrottleRaw();
        doc["powerPercent"] = power.getPower();
        doc["motorTempMc"] = telemetry.getMotorTempMilliCelsius();
        if (isRpmAvailable()) {
            doc["rpm"] = telemetry.getRpm();
        }
        if (isCurrentAvailable()) {
            doc["escCurrentMa"] = batteryCurrentMa;
        }
        doc["escTempMc"] = telemetry.getEscTempMilliCelsius();
        doc["armed"] = throttle.isArmed();
        doc["uptimeMs"] = millis();
        doc["lastTelemetryUpdateMs"] = telemetry.getLastUpdate();

        // Generic Bluetooth BMS data when available
        if (isBmsDataAvailable()) {
            JsonObject bms = doc.createNestedObject("bms");
            bms["available"] = true;
            if (bluetoothBms.getTempCount() > 0) {
                int16_t maxTemp = bluetoothBms.getTempCelsius(0);
                for (uint8_t i = 1; i < bluetoothBms.getTempCount(); i++) {
                    int16_t t = bluetoothBms.getTempCelsius(i);
                    if (t > maxTemp) maxTemp = t;
                }
                bms["tempMaxC"] = maxTemp;
            }
            if (isBmsCellDataAvailable()) {
                bms["cellMinMv"] = bluetoothBms.getCellMinMilliVolts();
                bms["cellMaxMv"] = bluetoothBms.getCellMaxMilliVolts();
                bms["cellDeltaMv"] = bluetoothBms.getCellDeltaMilliVolts();
            }
        }

        if (doc.overflowed()) {
            request->send(500, "text/plain", "Estouro do buffer JSON");
            return;
        }

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    server.on("/config.css", HTTP_GET, [](AsyncWebServerRequest *request){
        logWebHeap("/config.css");
        request->send(200, "text/css; charset=utf-8", reinterpret_cast<const uint8_t*>(COMMON_CSS), strlen(COMMON_CSS));
    });

    server.on("/config-power.js", HTTP_GET, [](AsyncWebServerRequest *request){
        logWebHeap("/config-power.js");
        request->send(200, "application/javascript; charset=utf-8", reinterpret_cast<const uint8_t*>(CONFIG_POWER_PAGE_JS), strlen_P(CONFIG_POWER_PAGE_JS));
    });

    server.on("/config-thermal.js", HTTP_GET, [](AsyncWebServerRequest *request){
        logWebHeap("/config-thermal.js");
        request->send(200, "application/javascript; charset=utf-8", reinterpret_cast<const uint8_t*>(CONFIG_THERMAL_PAGE_JS), strlen_P(CONFIG_THERMAL_PAGE_JS));
    });

    server.on("/config-bms.js", HTTP_GET, [](AsyncWebServerRequest *request){
        logWebHeap("/config-bms.js");
        request->send(200, "application/javascript; charset=utf-8", reinterpret_cast<const uint8_t*>(CONFIG_BMS_PAGE_JS), strlen_P(CONFIG_BMS_PAGE_JS));
    });

    server.on("/telemetry.js", HTTP_GET, [](AsyncWebServerRequest *request){
        logWebHeap("/telemetry.js");
        request->send(200, "application/javascript; charset=utf-8", reinterpret_cast<const uint8_t*>(TELEMETRY_PAGE_JS), strlen_P(TELEMETRY_PAGE_JS));
    });

    server.on("/config/power", HTTP_GET, [](AsyncWebServerRequest *request){
        const size_t len = strlen_P(CONFIG_POWER_PAGE_HTML);
        logWebHeap("/config/power");
        request->send(200, "text/html; charset=utf-8", reinterpret_cast<const uint8_t*>(CONFIG_POWER_PAGE_HTML), len);
    });

    server.on("/config/thermal", HTTP_GET, [](AsyncWebServerRequest *request){
        const size_t len = strlen_P(CONFIG_THERMAL_PAGE_HTML);
        logWebHeap("/config/thermal");
        request->send(200, "text/html; charset=utf-8", reinterpret_cast<const uint8_t*>(CONFIG_THERMAL_PAGE_HTML), len);
    });

    server.on("/config/bms", HTTP_GET, [](AsyncWebServerRequest *request){
        const size_t len = strlen_P(CONFIG_BMS_PAGE_HTML);
        logWebHeap("/config/bms");
        request->send(200, "text/html; charset=utf-8", reinterpret_cast<const uint8_t*>(CONFIG_BMS_PAGE_HTML), len);
    });

    // Configuration page - register AFTER /config/values and /config/save to avoid route conflicts
    server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request){
        logWebHeap("/config");
        request->send(200, "text/html", renderConfigHubPage());
    });

    server.on("/telemetry", HTTP_GET, [](AsyncWebServerRequest *request){
        const size_t len = strlen_P(TELEMETRY_PAGE_HTML);
        logWebHeap("/telemetry");
        request->send(200, "text/html; charset=utf-8", reinterpret_cast<const uint8_t*>(TELEMETRY_PAGE_HTML), len);
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

                // Only list log files
                if(fileName.endsWith(".csv") || fileName.endsWith(".txt")) {
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
        if (!checkPin(request)) { request->send(403, "text/plain", "Invalid PIN"); return; }
        if(request->hasParam("file")){
            String filename = request->getParam("file")->value();
            // Security: reject path traversal and ensure absolute path
            if (filename.indexOf("..") >= 0) {
                request->send(400, "text/plain", "Caminho inválido");
                return;
            }
            if(!filename.startsWith("/")) filename = "/" + filename;

            if(LittleFS.exists(filename)){
                LittleFS.remove(filename);
                request->send(200, "text/plain", "Excluído");
            } else {
                request->send(404, "text/plain", "Arquivo não encontrado");
            }
        } else {
            request->send(400, "text/plain", "Parâmetro de arquivo ausente");
        }
    });

    server.on("/delete-all-logs", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!checkPin(request)) { request->send(403, "text/plain", "Invalid PIN"); return; }
        File root = LittleFS.open("/");
        if (!root) {
            request->send(500, "text/plain", "Erro no sistema de arquivos");
            return;
        }
        std::vector<String> toDelete;
        File file = root.openNextFile();
        while (file) {
            String fileName = String(file.name());
            if (!fileName.startsWith("/")) {
                fileName = "/" + fileName;
            }
            if (fileName.endsWith(".csv") || fileName.endsWith(".txt")) {
                toDelete.push_back(fileName);
            }
            file = root.openNextFile();
        }
        root.close();
        for (const String& name : toDelete) {
            LittleFS.remove(name);
        }
        logger.afterLogFilesClearedFromStorage();
        request->send(200, "text/plain", "OK");
    });

    // Handle firmware update POST request
    server.on(
        "/update",
        HTTP_POST,
        [](AsyncWebServerRequest *request) {
            // This is the 'final' callback for the POST request, after file upload is complete.
            request->send(200, "text/plain", (Update.hasError()) ? "Falha na atualização!" : "Atualização de firmware em andamento. O dispositivo será reiniciado.");
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
    ElegantOTA.begin(&server);
    ElegantOTA.setAuth("admin", settings.getConfigPin().c_str());
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
    if (isActive) {
        ElegantOTA.loop(); // Process ElegantOTA events only if the server is active
        dnsServer.processNextRequest(); // Only process DNS when WiFi is active
    }
}
