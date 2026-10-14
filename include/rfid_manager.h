#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Adafruit_PN532.h>
#include "pins_config.h"

class RfidManager {
public:
    static RfidManager& instance();

    bool begin();
    void update();

private:
    RfidManager();

    void pollMFRC522();
    void pollPN532();

    MFRC522 mfrc522_;
    Adafruit_PN532 pn532_;

    bool mfrc522Ready_ = false;
    bool pn532Ready_ = false;

    uint32_t lastPollTime_ = 0;
    uint8_t pollTurn_ = 0; // 0: MFRC522, 1: PN532
};
