# IC 卡门禁 PCB · 原理图连线规格（嘉立创EDA）

工程：`IC卡门禁-课设`（EasyEDA Pro）  
备份工程：`hardware/ic_access_control`（KiCad，待导入）  
工艺：2 层 1.6mm，嘉立创打样；阻容 0603

## 1. 已选器件（LCSC）

| 位号 | 器件 | LCSC | 封装 |
|------|------|------|------|
| U1 | ESP32-WROOM-32 | C95209 | 模组 |
| U2 | MFRC52202HN1,151 | C11322 | QFN-32 |
| U3 | PN532 NFC Module v4 | C9900233104 | 模组 |
| U4 | TP4056X-42-ESOP8 | C2763448 | SOP-8 |
| U5 | AMS1117-3.3 | C5205141 | SOT-223 |
| U6 | CH340C | C84681 | SOP-16 |
| J1 | USB-C 16P UCB11L69P16 | C9900009190 | SMD |
| U7 | DW01 | C359989 | SOT-23-6 |
| Q1 | FS8205A | C14212 | TSSOP-8 |
| Q2/Q3 | S8050 | C2931719 | SOT-23 |
| DS1 | 0.96" OLED 4P | C284855 | 排针 |
| LS1 | 蜂鸣器 SMD | C9900006792 | 5.2mm |
| D1 | SS14 | C9900013398 | SOD-323 |
| K1 | SRD-03VDC-SL-C | C24585 | 直插 |
| SW1-3 | 轻触按键 3×4 | C9900017573 | SMD |
| LED1-3 | 0603 LED 绿/红/蓝 | — | 0603 |

另需：100nF×10、10µF×4、1k×6、10k×5、5.1k×2（CC）、电池座 JST-PH 2.0、2P 3.5mm 端子

## 2. ESP32 引脚分配

| ESP32 | 网络 | 去向 |
|-------|------|------|
| 3V3 | +3V3 | LDO 输出 |
| GND | GND | 系统地 |
| EN | EN | 10k 上拉 + 100nF + 复位键 |
| IO0 | BOOT | 按键对地（下载） |
| TXD0 (IO1) | UART_TX | CH340 RXD |
| RXD0 (IO3) | UART_RX | CH340 TXD |
| IO18 | SPI_SCK | MFRC522 + PN532 |
| IO19 | SPI_MISO | MFRC522 + PN532 |
| IO23 | SPI_MOSI | MFRC522 + PN532 |
| IO5 | RC_SS | MFRC522 NSS |
| IO15 | PN_SS | PN532 SS |
| IO14 | RC_RST | MFRC522 NRST（可选） |
| IO21 | SDA | OLED |
| IO22 | SCL | OLED |
| IO25 | BUZ | Q2 基极（经 1k） |
| IO26 | LED_G | LED1 |
| IO27 | LED_R | LED2 |
| IO32 | LED_S | LED3 |
| IO33 | SW1 | 管理键（10k 上拉） |
| IO4 | SW2 | 加卡 |
| IO13 | SW3 | 减卡/确认 |
| IO17 | RLY | Q3 基极（经 1k） |

## 3. 电源

```
USB-C VBUS ──► TP4056 VIN
TP4056 BAT ──► BAT+ ──► AMS1117 VIN
TP4056 GND ──► 保护地(经 FS8205A/DW01) ──► GND
AMS1117 VOUT ──► +3V3 ──► ESP32 / MFRC522 / PN532 / OLED / CH340
```

- USB-C：CC1/CC2 各 5.1k 到 GND
- TP4056：PROG 电阻 1.2k（约 1A）或 10k（约 130mA 课设更安全）
- DW01+FS8205：电池过充/过放/过流保护
- 继电器 K1 线圈 3.3V，经 Q3 低边驱动，D1 续流

## 4. 读卡

**MFRC522（实体 Mifare）**
- SPI：SCK/MISO/MOSI/NSS = 18/19/23/5
- TX1/TX2 → PCB 天线 + 匹配电容（NXP AN1444 参考；模块可省）
- 天线区净空，勿铺铜

**PN532 模组（手机 HCE）**
- SPI 共总线，SS=15
- 模组自带天线，板边放置朝外

## 5. 人机与执行

- OLED：VCC/GND/SDA/SCL = 3V3/GND/21/22
- 蜂鸣器：GPIO25 → 1k → Q2.B；Q2.E=GND；蜂鸣器+ 接 3V3，− 接 Q2.C
- LED：GPIO → 1k → LED → GND
- 按键：GPIO → 按键 → GND，10k 上拉到 3V3
- 继电器：GPIO17 → 1k → Q3.B；线圈 3V3—Q3.C；D1 并线圈；NO/COM 出 2P 端子

## 6. PCB 现状（2026-09-23）

嘉立创工程 `IC卡门禁-课设` / Board1 / PCB1 已保存：

| 项目 | 状态 |
|------|------|
| 器件 | 51 封装，位号 C1–C14、R1–R16、U1–U7、J1、Q1–Q3、DS1、LS1、D1、K1、SW1–3、LED1–3 |
| 板框 | 120×80 mm（4724×3150 mil），Board Outline 层矩形 |
| 布局 | 功能分区，封装外框零重叠 |
| 网络 | 按本规格书手工赋到焊盘（原理图未形成电气网表） |
| 布线 | 42 网络 / 约 246 段顶层层曼哈顿线 + MFRC522 矩形环天线 |
| 安装孔 | 四角 M3（H1–H4，钻孔 3.2 mm） |
| DRC | **未清零**：自动布线 API 超时，手写曼哈顿走线有大量同层间距错误，需在 EDA 里推挤/重布关键网络 |

### 已知缺口

1. **原理图无电气连接**：网络标号未形成网表，PCB 网络为 API 手工赋予；后续改原理图需重新同步
2. **MFRC522 缺 27.12 MHz 晶振**：裸片需要 Y1 + 负载电容，BOM 未列
3. **缺接插件**：电池 JST-PH、继电器 2P 端子未在 51 器件内
4. **C10=15pF** 为天线匹配占位，按 NXP AN1444/AN1445 实测再定
5. **阻容取值**：LED/三极管基极限流已标 1k，其余按上表；请再对一遍原理图 Value
6. **U3 PN532** 脚序按常见 v4 模组（3V3/GND/MOSI/MISO/SCK/SS/IRQ/RST…）假定，焊接前请对模组丝印
7. **ESP32 天线净空 / MFRC522 天线区** 需在 EDA 禁止铺铜，当前未建 keepout 区

## 7. KiCad 备份与可商用评估

- 嘉立创导出 Protel ASCII → `hardware/protel2kicad.py` → `hardware/ic_access_control.kicad_pcb`
- KiCad 10 DRC 报告：`hardware/kicad_drc_baseline.json`（948 项 / 31 未连接）
- 可商用缺口与门禁见 **`hardware/COMMERCIAL_READINESS.md`**
- 打样文件：`fab/IC_Card_Access_Gerber.zip` + BOM + PnP
