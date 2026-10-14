#pragma once

#include <Arduino.h>
#include <ESP32Servo.h>
#include "pins_config.h"
#include "access_controller.h"

enum class BuzzerPattern : uint8_t {
    NONE = 0,
    SHORT_BEEP,      // 成功一声短鸣 (100ms)
    ERROR_BEEP,      // 失败错误双鸣 (100ms on, 80ms off, 100ms on)
    LONG_BEEP        // 长鸣
};

class BspBoard {
public:
    static BspBoard& instance();

    void begin();
    void update();

    // 硬件动作触发
    void setRelay(bool energize);
    void setServo(bool unlock);
    void playBuzzer(BuzzerPattern pattern);
    void setLedGreen(bool on);
    void setLedRed(bool on);
    void setLedStatus(bool on);

    // 按键事件轮询
    void checkButtons();

private:
    BspBoard() = default;

    // 舵机控制
    Servo lockServo_;

    // 蜂鸣器异步非阻塞播放状态机
    BuzzerPattern currentBuzzer_ = BuzzerPattern::NONE;
    uint32_t buzzerStepTime_ = 0;
    uint8_t buzzerStep_ = 0;

    // 按键消抖
    uint32_t lastDebounceTime_ = 0;
    bool lastSw1State_ = HIGH;
    bool lastSw2State_ = HIGH;
    bool lastSw3State_ = HIGH;
};
