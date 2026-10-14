#include "rfid_manager.h"
#include "access_controller.h"

RfidManager::RfidManager()
    : mfrc522_(PIN_RC_SS, PIN_RC_RST),
      pn532_(PIN_PN_SS, &SPI) {
}

RfidManager& RfidManager::instance() {
    static RfidManager inst;
    return inst;
}

bool RfidManager::begin() {
    // 强制将两个 SPI 片选先拉高，防止上电总线竞争
    pinMode(PIN_RC_SS, OUTPUT);
    digitalWrite(PIN_RC_SS, HIGH);
    pinMode(PIN_PN_SS, OUTPUT);
    digitalWrite(PIN_PN_SS, HIGH);

    // 启动硬件 SPI 总线
    SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);

    // 1. 初始化 MFRC522 (实体 Mifare 卡)
    digitalWrite(PIN_PN_SS, HIGH);
    digitalWrite(PIN_RC_SS, LOW);
    mfrc522_.PCD_Init();
    delay(20);
    byte mfrcVer = mfrc522_.PCD_ReadRegister(mfrc522_.VersionReg);
    digitalWrite(PIN_RC_SS, HIGH);

    if (mfrcVer != 0x00 && mfrcVer != 0xFF) {
        Serial.printf("[RFID] MFRC522 detected! Version: 0x%02X\n", mfrcVer);
        mfrc522Ready_ = true;
    } else {
        Serial.println("[RFID] Warning: MFRC522 not responding (check wiring/crystal).");
        mfrc522Ready_ = false;
    }

    // 2. 初始化 PN532 (手机 HCE 虚拟卡)
    digitalWrite(PIN_RC_SS, HIGH);
    pn532_.begin();
    uint32_t pnVer = pn532_.getFirmwareVersion();
    if (pnVer) {
        Serial.printf("[RFID] PN532 detected! Firmware ver: %d.%d\n", 
                      (int)((pnVer >> 16) & 0xFF), 
                      (int)((pnVer >> 8) & 0xFF));
        // 配置为 SAM 模式 (Normal mode)
        pn532_.SAMConfig();
        // 设置非阻塞重试计数：只读一次立即返回，防止轮询卡顿
        pn532_.setPassiveActivationRetries(0x01);
        pn532Ready_ = true;
    } else {
        Serial.println("[RFID] Warning: PN532 module not responding.");
        pn532Ready_ = false;
    }

    return (mfrc522Ready_ || pn532Ready_);
}

void RfidManager::update() {
    uint32_t now = millis();
    if (now - lastPollTime_ < 60) {
        return; // 每 60ms 轮转交替查询一次
    }
    lastPollTime_ = now;

    if (pollTurn_ == 0) {
        if (mfrc522Ready_) {
            pollMFRC522();
        }
        pollTurn_ = 1;
    } else {
        if (pn532Ready_) {
            pollPN532();
        }
        pollTurn_ = 0;
    }
}

void RfidManager::pollMFRC522() {
    // 确保 PN532 片选释放
    digitalWrite(PIN_PN_SS, HIGH);

    if (!mfrc522_.PICC_IsNewCardPresent()) {
        return;
    }
    if (!mfrc522_.PICC_ReadCardSerial()) {
        return;
    }

    // 格式化 UID 为十六进制大写字符串
    String uidStr = "";
    for (byte i = 0; i < mfrc522_.uid.size; i++) {
        if (mfrc522_.uid.uidByte[i] < 0x10) uidStr += "0";
        uidStr += String(mfrc522_.uid.uidByte[i], HEX);
    }
    uidStr.toUpperCase();

    // 停止读卡并退出加密态
    mfrc522_.PICC_HaltA();
    mfrc522_.PCD_StopCrypto1();

    // 分发给门禁核心处理
    AccessController::instance().handleCardDetected(uidStr, "RC522");
}

void RfidManager::pollPN532() {
    // 确保 MFRC522 片选释放
    digitalWrite(PIN_RC_SS, HIGH);

    uint8_t uid[7];
    uint8_t uidLength = 0;

    // 尝试读取 ISO14443A 协议目标（包含手机 HCE 虚拟卡）
    bool success = pn532_.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 30);
    if (success && uidLength > 0) {
        String uidStr = "";
        for (uint8_t i = 0; i < uidLength; i++) {
            if (uid[i] < 0x10) uidStr += "0";
            uidStr += String(uid[i], HEX);
        }
        uidStr.toUpperCase();

        AccessController::instance().handleCardDetected(uidStr, "PN532");
    }
}
