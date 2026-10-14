#include "homekit_service.h"
#include <HomeSpan.h>
#include "access_controller.h"

struct DEV_DoorLock : Service::LockMechanism {
    SpanCharacteristic *current;
    SpanCharacteristic *target;

    DEV_DoorLock() : Service::LockMechanism() {
        current = new Characteristic::LockCurrentState(1); // 1 = Secured (关门)
        target = new Characteristic::LockTargetState(1);  // 1 = Secured
    }

    boolean update() override {
        if (target->getNewVal() == 0) { // 0 = Unsecured (点按开门)
            AccessController::instance().unlockRemote("HomeKit");
            current->setVal(0);
        } else {
            current->setVal(1);
        }
        return true;
    }

    void loop() override {
        // 当物理门禁已自动闭锁，同步 HomeKit 状态回 Secured
        if (AccessController::instance().getLockState() == LockState::LOCKED && current->getVal() != 1) {
            current->setVal(1);
            target->setVal(1);
        }
    }
};

HomeKitService& HomeKitService::instance() {
    static HomeKitService inst;
    return inst;
}

void HomeKitService::begin() {
    // 默认启用 HomeKit 软件 HAP 服务
    homeSpan.setLogLevel(1);
    homeSpan.setStatusPin(PIN_LED_S); // 用蓝色状态灯提示配对状态
    homeSpan.begin(Category::Locks, "IC Door Access", "IC-Access-01", "DoorLock");

    new SpanAccessory();
        new Service::AccessoryInformation();
            new Characteristic::Identify();
            new Characteristic::Manufacturer("MiMo Access");
            new Characteristic::SerialNumber("SN-2026-001");
            new Characteristic::Model("ESP32-HAP-Lock");
            new Characteristic::FirmwareRevision("1.0.0");
        new DEV_DoorLock();

    started_ = true;
    enabled_ = true;
    Serial.println("[HomeKit] HomeSpan software HAP accessory initialized.");
}

void HomeKitService::update() {
    if (started_ && enabled_) {
        homeSpan.poll();
    }
}
