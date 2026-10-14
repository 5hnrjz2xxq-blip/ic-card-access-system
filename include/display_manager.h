#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "pins_config.h"
#include "access_controller.h"

class DisplayManager {
public:
    static DisplayManager& instance();

    bool begin();
    void update();
    void showStatus(const char* title, const char* detail, const char* footer = nullptr);

private:
    DisplayManager();

    Adafruit_SSD1306 display_;
    bool available_ = false;
    uint32_t lastRefreshTime_ = 0;

    String currentTitle_ = "DOOR ACCESS";
    String currentDetail_ = "Ready to Swipe";
    String currentFooter_ = "Cards: 0";
};
