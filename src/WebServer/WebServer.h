#ifndef WEBSERVER_H
#define WEBSERVER_H

class WebServer {
public:
    WebServer();
    void begin();
    void stop();
    void handleClient();

private:
    // Private members will be added here
};

#endif // WEBSERVER_H
