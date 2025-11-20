#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>

class WebServer {
public:
    WebServer();
    void begin();
    void stop();
    void handleClient();

private:
    AsyncWebServer server;
    DNSServer dnsServer;
};

#endif // WEBSERVER_H
