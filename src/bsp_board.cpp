#include "bsp_board.h"

BspBoard& BspBoard::instance() {
    static BspBoard inst;
    return inst;
}

void BspBoard::begin() {
    // 继电器初始化（默认断开）
    pinMode(PIN_RELAY, OUTPUT);
    digitalWrite(PIN_RELAY, LOW);

    // 舵机初始化（50Hz 周期，默认 0° 闭锁位置）
    lockServo_.setPeriodHertz(50);
    lockServo_.attach(PIN_SERVO, 500, 2400);
    lockServo_.write(SERVO_LOCKED_ANGLE);

    // 蜂鸣器初始化（默认静音）
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);

    // LED 初始化（低电平熄灭）
    pinMode(PIN_LED_G, OUTPUT);
    pinMode(PIN_LED_R, OUTPUT);
    pinMode(PIN_LED_S, OUTPUT);
    digitalWrite(PIN_LED_G, LOW);
    digitalWrite(PIN_LED_R, LOW);
    digitalWrite(PIN_LED_S, HIGH); // 蓝灯常亮表示系统就绪

    // 按键输入初始化（启用内部上拉）
    pinMode(PIN_SW_ADMIN, INPUT_PULLUP);
    pinMode(PIN_SW_ADD, INPUT_PULLUP);
    pinMode(PIN_SW_DEL, INPUT_PULLUP);

    // 注册多监听事件响应
    AccessController::instance().addListener([this](const AccessEvent& ev) {
        switch (ev.type) {
            case AccessEventType::SWIPE_PASS:
                this->setRelay(true);
                this->setServo(true); // 舵机旋转到 90° 开门
                this->setLedGreen(true);
                this->setLedRed(false);
                this->playBuzzer(BuzzerPattern::SHORT_BEEP);
                break;

            case AccessEventType::SWIPE_REJECT:
                this->setRelay(false);
                this->setServo(false);
                this->setLedRed(true);
                this->setLedGreen(false);
                this->playBuzzer(BuzzerPattern::ERROR_BEEP);
                break;

            case AccessEventType::DOOR_RELOCKED:
                this->setRelay(false);
                this->setServo(false); // 舵机回转到 0° 锁门
                this->setLedGreen(false);
                this->setLedRed(false);
                break;

            case AccessEventType::ADMIN_ADD_SUCCESS:
            case AccessEventType::ADMIN_DEL_SUCCESS:
                this->playBuzzer(BuzzerPattern::SHORT_BEEP);
                break;

            case AccessEventType::ADMIN_ADD_FAIL:
            case AccessEventType::ADMIN_DEL_FAIL:
                this->playBuzzer(BuzzerPattern::ERROR_BEEP);
                break;

            case AccessEventType::MODE_CHANGED:
                this->playBuzzer(BuzzerPattern::SHORT_BEEP);
                break;
        }
    });
}

void BspBoard::setRelay(bool energize) {
    digitalWrite(PIN_RELAY, energize ? HIGH : LOW);
}

void BspBoard::setServo(bool unlock) {
    lockServo_.write(unlock ? SERVO_UNLOCKED_ANGLE : SERVO_LOCKED_ANGLE);
}

void BspBoard::setLedGreen(bool on) {
    digitalWrite(PIN_LED_G, on ? HIGH : LOW);
}

void BspBoard::setLedRed(bool on) {
    digitalWrite(PIN_LED_R, on ? HIGH : LOW);
}

void BspBoard::setLedStatus(bool on) {
    digitalWrite(PIN_LED_S, on ? HIGH : LOW);
}

void BspBoard::playBuzzer(BuzzerPattern pattern) {
    currentBuzzer_ = pattern;
    buzzerStep_ = 0;
    buzzerStepTime_ = millis();
}

void BspBoard::update() {
    checkButtons();

    // 蜂鸣器非阻塞节拍驱动
    if (currentBuzzer_ == BuzzerPattern::SHORT_BEEP) {
        if (buzzerStep_ == 0) {
            digitalWrite(PIN_BUZZER, HIGH);
            if (millis() - buzzerStepTime_ >= 100) {
                digitalWrite(PIN_BUZZER, LOW);
                currentBuzzer_ = BuzzerPattern::NONE;
            }
        }
    } else if (currentBuzzer_ == BuzzerPattern::ERROR_BEEP) {
        uint32_t elapsed = millis() - buzzerStepTime_;
        if (buzzerStep_ == 0) {
            digitalWrite(PIN_BUZZER, HIGH);
            if (elapsed >= 100) {
                digitalWrite(PIN_BUZZER, LOW);
                buzzerStep_ = 1;
                buzzerStepTime_ = millis();
            }
        } else if (buzzerStep_ == 1) {
            if (elapsed >= 80) {
                digitalWrite(PIN_BUZZER, HIGH);
                buzzerStep_ = 2;
                buzzerStepTime_ = millis();
            }
        } else if (buzzerStep_ == 2) {
            if (elapsed >= 100) {
                digitalWrite(PIN_BUZZER, LOW);
                currentBuzzer_ = BuzzerPattern::NONE;
                setLedRed(false);
            }
        }
    } else if (currentBuzzer_ == BuzzerPattern::LONG_BEEP) {
        if (millis() - buzzerStepTime_ < 500) {
            digitalWrite(PIN_BUZZER, HIGH);
        } else {
            digitalWrite(PIN_BUZZER, LOW);
            currentBuzzer_ = BuzzerPattern::NONE;
        }
    }
}

void BspBoard::checkButtons() {
    // 简易按键消抖（每 40ms 采样一次）
    if (millis() - lastDebounceTime_ < 40) return;
    lastDebounceTime_ = millis();

    bool sw1 = digitalRead(PIN_SW_ADMIN);
    bool sw2 = digitalRead(PIN_SW_ADD);
    bool sw3 = digitalRead(PIN_SW_DEL);

    // SW1: 管理模式开关
    if (lastSw1State_ == HIGH && sw1 == LOW) {
        if (AccessController::instance().getAdminMode() == AdminMode::NORMAL) {
            AccessController::instance().enterAdminMode(AdminMode::WAIT_ADD);
        } else {
            AccessController::instance().exitAdminMode();
        }
    }

    // SW2: 切换到加卡态
    if (lastSw2State_ == HIGH && sw2 == LOW) {
        AccessController::instance().enterAdminMode(AdminMode::WAIT_ADD);
    }

    // SW3: 切换到删卡态
    if (lastSw3State_ == HIGH && sw3 == LOW) {
        AccessController::instance().enterAdminMode(AdminMode::WAIT_DEL);
    }

    lastSw1State_ = sw1;
    lastSw2State_ = sw2;
    lastSw3State_ = sw3;
}
