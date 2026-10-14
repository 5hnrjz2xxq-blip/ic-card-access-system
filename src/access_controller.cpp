#include "access_controller.h"
#include "pins_config.h"

AccessController& AccessController::instance() {
    static AccessController inst;
    return inst;
}

void AccessController::begin() {
    StorageManager::instance().begin();
    lockState_ = LockState::LOCKED;
    adminMode_ = AdminMode::NORMAL;
    lastUid_ = "";
    lastReadTime_ = 0;
}

void AccessController::enterAdminMode(AdminMode mode) {
    adminMode_ = mode;
    const char* msg = (mode == AdminMode::WAIT_ADD) ? "ADMIN: Swipe to ADD" : "ADMIN: Swipe to DEL";
    notifyEvent(AccessEventType::MODE_CHANGED, "", "ADMIN", msg);
    Serial.printf("[Controller] Entered %s\n", msg);
}

void AccessController::exitAdminMode() {
    adminMode_ = AdminMode::NORMAL;
    notifyEvent(AccessEventType::MODE_CHANGED, "", "ADMIN", "Mode: NORMAL");
    Serial.println("[Controller] Exited Admin Mode -> NORMAL");
}

void AccessController::handleCardDetected(const String& uidHex, const char* source) {
    uint32_t now = millis();

    // 防重读防抖滤波：同卡在 CARD_REPEAT_INTERVAL_MS 毫秒内刷入则忽略
    if (uidHex.equalsIgnoreCase(lastUid_) && (now - lastReadTime_ < CARD_REPEAT_INTERVAL_MS)) {
        Serial.printf("[Controller] Debounce filtered: %s swipe within %u ms\n", 
                      uidHex.c_str(), (unsigned int)(now - lastReadTime_));
        return;
    }

    lastUid_ = uidHex;
    lastReadTime_ = now;

    Serial.printf("[Controller] Card detected from [%s], UID: %s\n", source, uidHex.c_str());

    // 管理模式分支：加卡
    if (adminMode_ == AdminMode::WAIT_ADD) {
        bool added = StorageManager::instance().addCard(uidHex);
        StorageManager::instance().logAccess(uidHex, 
            added ? AccessResult::ADMIN_ADD : AccessResult::REJECT, source);
        if (added) {
            notifyEvent(AccessEventType::ADMIN_ADD_SUCCESS, uidHex, source, "Card Added OK");
        } else {
            notifyEvent(AccessEventType::ADMIN_ADD_FAIL, uidHex, source, "Add Fail / Full");
        }
        exitAdminMode();
        return;
    }

    // 管理模式分支：删卡
    if (adminMode_ == AdminMode::WAIT_DEL) {
        bool removed = StorageManager::instance().removeCard(uidHex);
        StorageManager::instance().logAccess(uidHex, 
            removed ? AccessResult::ADMIN_DEL : AccessResult::REJECT, source);
        if (removed) {
            notifyEvent(AccessEventType::ADMIN_DEL_SUCCESS, uidHex, source, "Card Removed OK");
        } else {
            notifyEvent(AccessEventType::ADMIN_DEL_FAIL, uidHex, source, "Not In Whitelist");
        }
        exitAdminMode();
        return;
    }

    // 正常刷卡模式：白名单鉴权
    if (StorageManager::instance().isAuthorized(uidHex)) {
        triggerUnlock(uidHex, source);
    } else {
        triggerReject(uidHex, source);
    }
}

void AccessController::unlockRemote(const char* sourceName) {
    Serial.printf("[Controller] Remote unlock commanded by: %s\n", sourceName);
    triggerUnlock("REMOTE", sourceName);
}

void AccessController::triggerUnlock(const String& uid, const char* source) {
    lockState_ = LockState::UNLOCKED;
    unlockStartTime_ = millis();

    StorageManager::instance().logAccess(uid, AccessResult::PASS, source);
    notifyEvent(AccessEventType::SWIPE_PASS, uid, source, "Access GRANTED");
    Serial.printf("[Controller] >>> DOOR UNLOCKED by %s [%s] <<<\n", uid.c_str(), source);
}

void AccessController::triggerReject(const String& uid, const char* source) {
    StorageManager::instance().logAccess(uid, AccessResult::REJECT, source);
    notifyEvent(AccessEventType::SWIPE_REJECT, uid, source, "Access DENIED");
    Serial.printf("[Controller] !!! ACCESS DENIED for %s [%s] !!!\n", uid.c_str(), source);
}

void AccessController::notifyEvent(AccessEventType type, const String& uid, const char* source, const char* msg) {
    AccessEvent ev;
    ev.type = type;
    ev.uid = uid;
    ev.source = source;
    ev.currentMode = adminMode_;
    ev.message = msg;

    for (auto& listener : listeners_) {
        if (listener) {
            listener(ev);
        }
    }
}

void AccessController::update() {
    // 门锁开门计时器：3秒后自动闭锁
    if (lockState_ == LockState::UNLOCKED) {
        if (millis() - unlockStartTime_ >= DOOR_UNLOCK_DURATION_MS) {
            lockState_ = LockState::LOCKED;
            notifyEvent(AccessEventType::DOOR_RELOCKED, "", "SYSTEM", "Door LOCKED");
            Serial.println("[Controller] >>> DOOR RELOCKED (Auto-timeout) <<<");
        }
    }
}
