#include "display_manager.h"
#include <Wire.h>

DisplayManager::DisplayManager()
    : display_(128, 64, &Wire, -1) {
}

DisplayManager& DisplayManager::instance() {
    static DisplayManager inst;
    return inst;
}

bool DisplayManager::begin() {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    if (!display_.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
        Serial.println("[Display] Warning: SSD1306 OLED not found or communication failed.");
        available_ = false;
        return false;
    }

    available_ = true;
    display_.clearDisplay();
    display_.setTextColor(SSD1306_WHITE);
    display_.setTextSize(1);
    display_.setCursor(16, 20);
    display_.println("Smart Access ESP32");
    display_.setCursor(24, 36);
    display_.println("Initializing...");
    display_.display();

    // 注册门禁控制器事件自动刷新屏幕
    AccessController::instance().addListener([this](const AccessEvent& ev) {
        char footerBuf[32];
        snprintf(footerBuf, sizeof(footerBuf), "[%s] Total:%d", 
                 ev.source.c_str(), 
                 (int)StorageManager::instance().getCardCount());

        switch (ev.type) {
            case AccessEventType::SWIPE_PASS:
                this->showStatus("ACCESS GRANTED", ev.uid.c_str(), footerBuf);
                break;

            case AccessEventType::SWIPE_REJECT:
                this->showStatus("ACCESS DENIED", ev.uid.c_str(), footerBuf);
                break;

            case AccessEventType::DOOR_RELOCKED:
                this->showStatus("DOOR LOCKED", "Ready to Swipe", footerBuf);
                break;

            case AccessEventType::ADMIN_ADD_SUCCESS:
                this->showStatus("CARD ADDED OK", ev.uid.c_str(), footerBuf);
                break;

            case AccessEventType::ADMIN_ADD_FAIL:
                this->showStatus("ADD FAILED", ev.uid.c_str(), footerBuf);
                break;

            case AccessEventType::ADMIN_DEL_SUCCESS:
                this->showStatus("CARD DELETED OK", ev.uid.c_str(), footerBuf);
                break;

            case AccessEventType::ADMIN_DEL_FAIL:
                this->showStatus("CARD NOT FOUND", ev.uid.c_str(), footerBuf);
                break;

            case AccessEventType::MODE_CHANGED:
                this->showStatus("MODE CHANGED", ev.message, footerBuf);
                break;
        }
    });

    return true;
}

void DisplayManager::showStatus(const char* title, const char* detail, const char* footer) {
    currentTitle_ = title ? title : "";
    currentDetail_ = detail ? detail : "";
    if (footer) currentFooter_ = footer;
    lastRefreshTime_ = millis();

    if (!available_) return;

    display_.clearDisplay();

    // 顶栏高亮背景条
    display_.fillRect(0, 0, 128, 14, SSD1306_WHITE);
    display_.setTextColor(SSD1306_BLACK);
    display_.setTextSize(1);
    display_.setCursor(4, 3);
    display_.print(currentTitle_);

    // 中间主要详情（卡号 / 提示）
    display_.setTextColor(SSD1306_WHITE);
    display_.setTextSize(1);
    display_.setCursor(4, 24);
    display_.print("UID: ");
    display_.println(currentDetail_.isEmpty() ? "None" : currentDetail_);

    // 门锁状态指示
    display_.setCursor(4, 38);
    if (AccessController::instance().getLockState() == LockState::UNLOCKED) {
        display_.print(">> DOOR UNLOCKED <<");
    } else {
        display_.print("Door: [ LOCKED ]");
    }

    // 底部分割线与统计
    display_.drawLine(0, 50, 127, 50, SSD1306_WHITE);
    display_.setCursor(4, 54);
    display_.print(currentFooter_);

    display_.display();
}

void DisplayManager::update() {
    if (!available_) return;

    // 如果 4 秒内无新刷卡事件，恢复待机屏显
    if (millis() - lastRefreshTime_ > 4000 && 
        AccessController::instance().getLockState() == LockState::LOCKED &&
        AccessController::instance().getAdminMode() == AdminMode::NORMAL) {
        
        char footerBuf[32];
        snprintf(footerBuf, sizeof(footerBuf), "Cards: %d (Ready)", 
                 (int)StorageManager::instance().getCardCount());
        showStatus("SMART ACCESS", "Swipe IC / NFC", footerBuf);
        lastRefreshTime_ = millis();
    }
}
