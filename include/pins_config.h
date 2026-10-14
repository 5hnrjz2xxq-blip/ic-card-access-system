#pragma once

#include <Arduino.h>

// ==========================================
// IC 卡门禁系统 · 硬件引脚分配
// 对齐 docs/PCB原理图连线规格.md
// ==========================================

// SPI 总线（MFRC522 与 PN532 共用）
constexpr int8_t PIN_SPI_SCK  = 18;
constexpr int8_t PIN_SPI_MISO = 19;
constexpr int8_t PIN_SPI_MOSI = 23;

// MFRC522 (实体 Mifare 卡读头)
constexpr int8_t PIN_RC_SS    = 5;
constexpr int8_t PIN_RC_RST   = 14;

// PN532 (手机 HCE 虚拟卡读头)
constexpr int8_t PIN_PN_SS    = 15;

// I2C 总线 (0.96" OLED SSD1306)
constexpr int8_t PIN_I2C_SDA  = 21;
constexpr int8_t PIN_I2C_SCL  = 22;
constexpr uint8_t OLED_I2C_ADDR = 0x3C;

// 声光交互
constexpr int8_t PIN_BUZZER   = 25;  // Q2 驱动高电平鸣响
constexpr int8_t PIN_LED_G    = 26;  // 绿色 LED：通过
constexpr int8_t PIN_LED_R    = 27;  // 红色 LED：拒绝/报警
constexpr int8_t PIN_LED_S    = 32;  // 蓝色/状态 LED

// 门锁执行机构
constexpr int8_t PIN_RELAY    = 17;  // Q3 驱动高电平吸合
constexpr int8_t PIN_SERVO    = 16;  // 舵机 PWM 信号线 (SG90 橙线)
constexpr int    SERVO_LOCKED_ANGLE   = 0;   // 闭锁角度
constexpr int    SERVO_UNLOCKED_ANGLE = 90;  // 开锁角度

// 人机按键（按下接地，内部/外部上拉）
constexpr int8_t PIN_SW_ADMIN = 33;  // SW1 管理模式
constexpr int8_t PIN_SW_ADD   = 4;   // SW2 录入白名单
constexpr int8_t PIN_SW_DEL   = 13;  // SW3 删除/确认

// 时间参数配置（毫秒）
constexpr uint32_t DOOR_UNLOCK_DURATION_MS = 3000; // 开门持续 3 秒后自动闭锁
constexpr uint32_t CARD_REPEAT_INTERVAL_MS = 2500; // 同一张卡刷卡防重读间隔 2.5 秒
constexpr size_t   MAX_WHITELIST_CAPACITY  = 30;   // 白名单最大容量
constexpr size_t   MAX_ACCESS_LOG_COUNT    = 20;   // 进出记录环形缓冲区大小
