#pragma once

#include <Arduino.h>

class HomeKitService {
public:
    static HomeKitService& instance();

    void begin();
    void update();
    bool isEnabled() const { return enabled_; }
    void setEnabled(bool en) { enabled_ = en; }

private:
    HomeKitService() = default;
    bool enabled_ = false;
    bool started_ = false;
};
