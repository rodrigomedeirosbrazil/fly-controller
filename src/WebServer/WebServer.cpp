#include "WebServer.h"
#include <ESPmDNS.h> // For mDNS support, often useful with web servers

const char* SOFT_AP_SSID = "FlyController";
// No password for open AP, suitable for captive portal-like behavior

// IP Address for the Soft AP
IPAddress apIP(192, 168, 4, 1);
IPAddress netMsk(255, 255, 255, 0);

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

    // Set up captive portal redirection for any request
    server.onNotFound([](AsyncWebServerRequest *request) {
        // Redirect all HTTP requests to the captive portal IP
        request->redirect("http://" + apIP.toString());
    });

    server.begin(); // Start the web server
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
