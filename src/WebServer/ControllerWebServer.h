#ifndef CONTROLLER_WEBSERVER_H
#define CONTROLLER_WEBSERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include <DNSServer.h>

class ControllerWebServer {
public:
    ControllerWebServer();
    void begin();
    void stop();
    void handleClient();

private:
    void startAP(); // Declare the private method
    bool isActive;
    AsyncWebServer server;
    DNSServer dnsServer;
};

#endif // CONTROLLER_WEBSERVER_H
