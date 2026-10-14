#include "storage_manager.h"
#include <Preferences.h>
#include <time.h>

static Preferences prefs;
static const char* PREF_NAMESPACE = "access_ctl";
static const char* KEY_WHITELIST = "whitelist";
static const char* KEY_RECORDS = "records";
static const char* KEY_RECORD_COUNT = "rec_cnt";
static const char* KEY_MASTER_CARD = "master_uid";

StorageManager& StorageManager::instance() {
    static StorageManager inst;
    return inst;
}

String StorageManager::normalizeUid(const String& uidHex) {
    String clean = uidHex;
    clean.trim();
    clean.toUpperCase();
    String res = "";
    for (size_t i = 0; i < clean.length(); ++i) {
        char c = clean.charAt(i);
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F')) {
            res += c;
        }
    }
    return res;
}

bool StorageManager::begin() {
    if (initialized_) return true;

    prefs.begin(PREF_NAMESPACE, false);
    loadWhitelist();
    loadRecords();

    // 如果白名单为空，注入两张演示用默认卡号（需求说明 6.1）
    if (whitelist_.empty()) {
        whitelist_.push_back("A3B2C1D0"); // 实体白卡示例
        whitelist_.push_back("04112233"); // 安卓 HCE 虚拟卡示例
        saveWhitelist();
        Serial.println("[Storage] Initialized default demo whitelist: A3B2C1D0, 04112233");
    }

    initialized_ = true;
    return true;
}

void StorageManager::loadWhitelist() {
    whitelist_.clear();
    if (!prefs.isKey(KEY_WHITELIST)) {
        return;
    }

    String packed = prefs.getString(KEY_WHITELIST, "");
    int start = 0;
    while (start < packed.length()) {
        int comma = packed.indexOf(',', start);
        String item;
        if (comma == -1) {
            item = packed.substring(start);
            start = packed.length();
        } else {
            item = packed.substring(start, comma);
            start = comma + 1;
        }
        item = normalizeUid(item);
        if (item.length() > 0) {
            whitelist_.push_back(item);
        }
    }
}

void StorageManager::saveWhitelist() {
    String packed = "";
    for (size_t i = 0; i < whitelist_.size(); ++i) {
        if (i > 0) packed += ",";
        packed += whitelist_[i];
    }
    prefs.putString(KEY_WHITELIST, packed);
}

void StorageManager::loadRecords() {
    logs_.clear();
    if (!prefs.isKey(KEY_RECORD_COUNT) || !prefs.isKey(KEY_RECORDS)) {
        return;
    }

    size_t count = prefs.getUInt(KEY_RECORD_COUNT, 0);
    if (count == 0 || count > MAX_ACCESS_LOG_COUNT) return;

    size_t totalBytes = count * sizeof(AccessRecord);
    std::vector<uint8_t> buffer(totalBytes);
    size_t readBytes = prefs.getBytes(KEY_RECORDS, buffer.data(), totalBytes);

    if (readBytes == totalBytes) {
        const AccessRecord* ptr = reinterpret_cast<const AccessRecord*>(buffer.data());
        logs_.assign(ptr, ptr + count);
        Serial.printf("[Storage] Loaded %d access records from NVS Flash.\n", (int)count);
    }
}

void StorageManager::saveRecords() {
    size_t count = logs_.size();
    prefs.putUInt(KEY_RECORD_COUNT, (uint32_t)count);
    if (count > 0) {
        prefs.putBytes(KEY_RECORDS, logs_.data(), count * sizeof(AccessRecord));
    } else {
        prefs.remove(KEY_RECORDS);
    }
}

bool StorageManager::isAuthorized(const String& uidHex) {
    String norm = normalizeUid(uidHex);
    for (const auto& item : whitelist_) {
        if (item.equalsIgnoreCase(norm)) {
            return true;
        }
    }
    return false;
}

bool StorageManager::addCard(const String& uidHex) {
    String norm = normalizeUid(uidHex);
    if (norm.isEmpty()) return false;
    if (isAuthorized(norm)) return true;

    if (whitelist_.size() >= MAX_WHITELIST_CAPACITY) {
        return false;
    }
    whitelist_.push_back(norm);
    saveWhitelist();
    return true;
}

bool StorageManager::removeCard(const String& uidHex) {
    String norm = normalizeUid(uidHex);
    for (auto it = whitelist_.begin(); it != whitelist_.end(); ++it) {
        if (it->equalsIgnoreCase(norm)) {
            whitelist_.erase(it);
            saveWhitelist();
            return true;
        }
    }
    return false;
}

std::vector<String> StorageManager::getAllCards() {
    return whitelist_;
}

size_t StorageManager::getCardCount() {
    return whitelist_.size();
}

void StorageManager::clearAllCards() {
    whitelist_.clear();
    saveWhitelist();
}

void StorageManager::logAccess(const String& uidHex, AccessResult result, const char* source, const char* formattedTime) {
    AccessRecord rec;
    memset(&rec, 0, sizeof(rec));

    time_t now = time(nullptr);
    rec.timestampSec = (now > 1600000000) ? (uint32_t)now : (millis() / 1000);

    if (formattedTime && strlen(formattedTime) > 0) {
        strncpy(rec.timeStr, formattedTime, sizeof(rec.timeStr) - 1);
    } else if (now > 1600000000) {
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        strftime(rec.timeStr, sizeof(rec.timeStr), "%m-%d %H:%M:%S", &timeinfo);
    } else {
        snprintf(rec.timeStr, sizeof(rec.timeStr), "+%us", (unsigned int)(millis() / 1000));
    }

    strncpy(rec.uid, normalizeUid(uidHex).c_str(), sizeof(rec.uid) - 1);
    rec.result = result;
    strncpy(rec.source, source ? source : "UNKNOWN", sizeof(rec.source) - 1);

    if (logs_.size() >= MAX_ACCESS_LOG_COUNT) {
        logs_.erase(logs_.begin()); // 保持最近 N 条环形缓冲
    }
    logs_.push_back(rec);
    saveRecords(); // 存入 NVS 保证掉电不丢失 (F8)
}

std::vector<AccessRecord> StorageManager::getRecentRecords(size_t limit) {
    if (limit > logs_.size()) {
        limit = logs_.size();
    }
    std::vector<AccessRecord> res;
    res.reserve(limit);
    for (size_t i = 0; i < limit; ++i) {
        res.push_back(logs_[logs_.size() - 1 - i]);
    }
    return res;
}

size_t StorageManager::getRecordCount() {
    return logs_.size();
}

void StorageManager::clearAllRecords() {
    logs_.clear();
    saveRecords();
}
