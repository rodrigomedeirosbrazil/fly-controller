#include "WebServer.h"
#include <ESPmDNS.h> // For mDNS support, often useful with web servers
#include <AsyncElegantOTA.h>

const char* SOFT_AP_SSID = "FlyController";
// No password for open AP, suitable for captive portal-like behavior

// IP Address for the Soft AP
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
    </style>
</head>
<body>
    <div class="container">
        <h1>FlyController</h1>
        <p>Você está conectado ao acelerador FlyController.</p>
        <p>Selecione um arquivo de firmware (.bin) para atualizar o dispositivo.</p>
        <form method="POST" action="/update" enctype="multipart/form-data">
            <input type="file" name="update" accept=".bin">
            <input type="submit" value="Atualizar">
        </form>
    </div>
</body>
</html>
)rawliteral";

WebServer::WebServer() : server(80) { // Initialize server on port 80
    // Constructor
}

void WebServer::begin() {
    startAP();
}

void WebServer::startAP() {
    Serial.println("Configuring access point...");
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, netMsk);
    WiFi.softAP(SOFT_AP_SSID);

    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());

    // Start DNS server for captive portal functionality
    dnsServer.start(53, "*", apIP); // DNS server will redirect all requests to apIP

    // Handle root URL
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", INDEX_HTML);
    });

    // Set up captive portal redirection for any request, prioritizing ElegantOTA
    server.onNotFound([](AsyncWebServerRequest *request) {
        if (request->method() == HTTP_GET) { // Only redirect GET requests for captive portal
            if(!AsyncElegantOTA.handleRequest(request)) { // Let ElegantOTA handle its requests first
                request->redirect("http://" + apIP.toString());
            }
        } else { // For POST, etc., send 404 or specific error
            request->send(404);
        }
    });

    server.begin(); // Start the web server
    AsyncElegantOTA.begin(&server); // Start ElegantOTA
    Serial.println("Web server started.");
}

void WebServer::stop() {
    Serial.println("Stopping web server and access point...");
    server.end(); // Stop the web server
    dnsServer.stop(); // Stop the DNS server
    WiFi.softAPdisconnect(true); // Disconnect clients and stop AP
    WiFi.mode(WIFI_OFF); // Turn off Wi-Fi
    Serial.println("Web server and access point stopped.");
}

void WebServer::handleClient() {
    // With AsyncWebServer, individual client handling is usually not needed in the main loop
    // However, the DNSServer might need to be serviced
    dnsServer.processNextRequest();
}
