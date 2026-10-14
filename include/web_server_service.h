#pragma once

#include <Arduino.h>
#include <WebServer.h>

class WebServerService {
public:
    static WebServerService& instance();

    void begin(uint16_t port = 80);
    void update();

private:
    WebServerService();

    void setupRoutes();
    void handleRoot();
    void handleUnlock();
    void handleStatus();
    void handleLogs();
    void handleWhitelist();
    void handleAddCard();
    void handleDeleteCard();

    WebServer server_;
    bool started_ = false;
};
