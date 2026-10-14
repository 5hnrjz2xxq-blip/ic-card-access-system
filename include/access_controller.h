#pragma once

#include <Arduino.h>
#include <vector>
#include <functional>
#include "storage_manager.h"
#include "pins_config.h"

enum class LockState : uint8_t {
    LOCKED = 0,
    UNLOCKED = 1
};

enum class AdminMode : uint8_t {
    NORMAL = 0,
    WAIT_ADD = 1,
    WAIT_DEL = 2
};

enum class AccessEventType : uint8_t {
    SWIPE_PASS = 0,
    SWIPE_REJECT,
    ADMIN_ADD_SUCCESS,
    ADMIN_ADD_FAIL,
    ADMIN_DEL_SUCCESS,
    ADMIN_DEL_FAIL,
    DOOR_RELOCKED,
    MODE_CHANGED
};

struct AccessEvent {
    AccessEventType type;
    String uid;
    String source;
    AdminMode currentMode;
    const char* message;
};

using AccessEventListener = std::function<void(const AccessEvent&)>;

class AccessController {
public:
    static AccessController& instance();

    void begin();
    void update(); // 状态机轮询

    // 核心卡片事件触发（由读头轮询或串口模拟调用）
    void handleCardDetected(const String& uidHex, const char* source);

    // 远程/网络开门触发（HomeKit / 快捷指令调用）
    void unlockRemote(const char* sourceName);

    // 管理模式控制
    void enterAdminMode(AdminMode mode);
    void exitAdminMode();
    AdminMode getAdminMode() const { return adminMode_; }
    LockState getLockState() const { return lockState_; }

    // 注册事件多观察者监听器（支持 BspBoard、DisplayManager 等多组件同时监听）
    void addListener(AccessEventListener listener) { 
        if (listener) listeners_.push_back(listener); 
    }

private:
    AccessController() = default;

    void triggerUnlock(const String& uid, const char* source);
    void triggerReject(const String& uid, const char* source);
    void notifyEvent(AccessEventType type, const String& uid, const char* source, const char* msg);

    LockState lockState_ = LockState::LOCKED;
    AdminMode adminMode_ = AdminMode::NORMAL;
    uint32_t unlockStartTime_ = 0;

    String lastUid_ = "";
    uint32_t lastReadTime_ = 0;

    std::vector<AccessEventListener> listeners_;
};
