#include <Arduino.h>
#include <time.h>
#include "pins_config.h"
#include "bsp_board.h"
#include "storage_manager.h"
#include "access_controller.h"
#include "display_manager.h"
#include "rfid_manager.h"
#include "homekit_service.h"
#include "web_server_service.h"

// 串口交互式模拟指令处理（在器件未到时支持纯串口软联调）
static void processSerialCommands() {
    if (!Serial.available()) return;

    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.isEmpty()) return;

    if (line.equalsIgnoreCase("HELP")) {
        Serial.println("\n--- [Serial Test Console Commands] ---");
        Serial.println("  SWIPE:<UID>   - Simulate card swipe (e.g. SWIPE:A3B2C1D0)");
        Serial.println("  UNLOCK        - Trigger remote unlock (Simulate HomeKit/NFC)");
        Serial.println("  LIST          - Display all whitelist cards and recent logs");
        Serial.println("  ADD:<UID>     - Add card to NVS whitelist");
        Serial.println("  DEL:<UID>     - Remove card from NVS whitelist");
        Serial.println("  CLEAR         - Clear all whitelist cards");
        Serial.println("  CLEARLOG      - Clear access history records");
        Serial.println("--------------------------------------\n");
    } else if (line.startsWith("SWIPE:")) {
        String uid = line.substring(6);
        AccessController::instance().handleCardDetected(uid, "SIM_UART");
    } else if (line.equalsIgnoreCase("UNLOCK")) {
        AccessController::instance().unlockRemote("SIM_UART");
    } else if (line.equalsIgnoreCase("LIST")) {
        Serial.println("\n=== Current Whitelist Cards ===");
        auto cards = StorageManager::instance().getAllCards();
        for (size_t i = 0; i < cards.size(); ++i) {
            Serial.printf("  [%d] %s\n", (int)i + 1, cards[i].c_str());
        }
        Serial.println("\n=== Recent Access Records (NVS Persistent) ===");
        auto records = StorageManager::instance().getRecentRecords(10);
        for (const auto& r : records) {
            Serial.printf("  [%s] UID: %s, Result: %d, Source: %s\n",
                          r.timeStr, r.uid, (int)r.result, r.source);
        }
        Serial.println("==============================================\n");
    } else if (line.startsWith("ADD:")) {
        String uid = line.substring(4);
        if (StorageManager::instance().addCard(uid)) {
            Serial.printf("[Console] Successfully added %s\n", uid.c_str());
        } else {
            Serial.printf("[Console] Failed to add %s\n", uid.c_str());
        }
    } else if (line.startsWith("DEL:")) {
        String uid = line.substring(4);
        if (StorageManager::instance().removeCard(uid)) {
            Serial.printf("[Console] Successfully removed %s\n", uid.c_str());
        } else {
            Serial.printf("[Console] Card %s not found in whitelist\n", uid.c_str());
        }
    } else if (line.equalsIgnoreCase("CLEAR")) {
        StorageManager::instance().clearAllCards();
        Serial.println("[Console] Whitelist cleared.");
    } else if (line.equalsIgnoreCase("CLEARLOG")) {
        StorageManager::instance().clearAllRecords();
        Serial.println("[Console] Access records cleared.");
    } else {
        Serial.printf("[Console] Unknown command '%s', type 'HELP' for instructions.\n", line.c_str());
    }
}

void setup() {
    Serial.begin(115200);
    delay(300);

    Serial.println("\n==============================================");
    Serial.println("  IC Card Access Control System (Plan A)");
    Serial.println("  Course Design: Digital Signal Processing");
    Serial.println("==============================================");

    // 1. 初始化板载 GPIO / 蜂鸣器 / 继电器 / 按键
    BspBoard::instance().begin();
    Serial.println("[Init] BSP pins and hardware drivers ready.");

    // 2. 初始化持久化存储（NVS 白名单与流水日志）
    StorageManager::instance().begin();
    Serial.printf("[Init] Storage ready. Whitelist: %d cards, Logs: %d records.\n", 
                  (int)StorageManager::instance().getCardCount(),
                  (int)StorageManager::instance().getRecordCount());

    // 3. 初始化门禁状态机与鉴权核心
    AccessController::instance().begin();
    Serial.println("[Init] AccessController state machine ready.");

    // 4. 初始化 0.96\" I2C OLED 显示屏
    DisplayManager::instance().begin();
    Serial.println("[Init] DisplayManager initialized.");

    // 5. 初始化 SPI 共享总线读卡器 (MFRC522 + PN532)
    RfidManager::instance().begin();
    Serial.println("[Init] RfidManager initialized (MFRC522 + PN532).");

    // 6. 初始化 HomeKit 软件 HAP 服务
    HomeKitService::instance().begin();
    Serial.println("[Init] HomeKit software service initialized.");

    // 7. 初始化 REST API WebServer（支撑 iPhone 快捷指令与局域网管理）
    WebServerService::instance().begin(80);
    Serial.println("[Init] HTTP REST API WebServer started on port 80.");

    // 8. 配置 NTP 自动对时（UTC+8 中国标准时间）
    configTime(8 * 3600, 0, "ntp.aliyun.com", "pool.ntp.org", "time.nist.gov");
    Serial.println("[Init] NTP time synchronization configured (UTC+8).");

    Serial.println("[System] Boot sequence complete. All subsystems active.");
    Serial.println("[System] Type 'HELP' in Serial Monitor for console test commands.");
}

void loop() {
    // 串口软仿真交互调试指令
    processSerialCommands();

    // 板载蜂鸣器状态机与按键扫描
    BspBoard::instance().update();

    // 门禁状态机轮询（3秒自动关门倒计时）
    AccessController::instance().update();

    // OLED 屏显待机画面刷新
    DisplayManager::instance().update();

    // 射频双读卡器分时轮询
    RfidManager::instance().update();

    // HomeKit 协议栈轮询
    HomeKitService::instance().update();

    // WebServer 请求轮询
    WebServerService::instance().update();
}
