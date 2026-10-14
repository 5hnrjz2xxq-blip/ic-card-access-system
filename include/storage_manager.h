#pragma once

#include <Arduino.h>
#include <vector>
#include "pins_config.h"

enum class AccessResult : uint8_t {
    PASS = 0,
    REJECT = 1,
    ADMIN_ADD = 2,
    ADMIN_DEL = 3
};

struct AccessRecord {
    uint32_t timestampSec; // Unix 时间戳或系统秒数
    char timeStr[20];      // 格式化时间字符串，如 "09-24 15:30:12" 或 "+35s"
    char uid[20];
    AccessResult result;
    char source[12];       // "RC522", "PN532", "HOMEKIT", "HTTP", "BUTTON"
};

class StorageManager {
public:
    static StorageManager& instance();

    bool begin();
    
    // 白名单管理（NVS 持久化）
    bool isAuthorized(const String& uidHex);
    bool addCard(const String& uidHex);
    bool removeCard(const String& uidHex);
    std::vector<String> getAllCards();
    size_t getCardCount();
    void clearAllCards();

    // 主控卡管理（NVS 持久化，刷此卡进入增删管理模式）
    String getMasterCard();
    bool setMasterCard(const String& uidHex);
    bool isMasterCard(const String& uidHex);

    // 历史进出流水记录（NVS 掉电持久化）
    void logAccess(const String& uidHex, AccessResult result, const char* source, const char* formattedTime = nullptr);
    std::vector<AccessRecord> getRecentRecords(size_t limit = 10);
    size_t getRecordCount();
    void clearAllRecords();

private:
    StorageManager() = default;
    void loadWhitelist();
    void saveWhitelist();
    void loadRecords();
    void saveRecords();
    String normalizeUid(const String& uidHex);

    std::vector<String> whitelist_;
    std::vector<AccessRecord> logs_;
    String masterCard_;           // 主控卡 UID，空字符串表示未设置
    bool initialized_ = false;
};
