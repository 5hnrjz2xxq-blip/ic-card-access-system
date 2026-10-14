# IC卡门禁系统设计

数字信号处理课程 · 电子系统设计实训

## 项目简介

基于单片机的 IC 卡门禁系统课设：读卡 → 鉴权 → 控门 → 提示/记录；  
门边**旁置演示**（不动家里原锁），并扩展 **Home App 开锁** 与 **手机 NFC 自动化开锁**。

## 技术选型（方案 A 已定稿）

| 模块 | 方案 |
|------|------|
| 主控 | **ESP32-WROOM-32E**（自制 PCB 模组上板） |
| 读卡 | **MFRC522** 实体 Mifare 卡 + **PN532** 读手机 NFC |
| 显示 | OLED I2C（SSD1306） |
| 执行 | 继电器模拟门锁（旁置，不接原锁） |
| 供电 | Type-C 充电 + 3.7V 锂电 + 3.3V LDO |
| 无线 | WiFi + BLE（模组自带） |
| HomeKit | 软件 HAP，家庭 App 门锁开锁（**非** MFi Home Key） |
| iPhone | Home App 开锁 + 碰墙贴 NFC → **快捷指令**开锁 |
| 安卓 / 鸿蒙 | **手机当 IC 卡被 PN532 直接刷开**（HCE），**不是**碰贴触发命令 |
| 开发环境 | VS Code + PlatformIO（**macOS** 编译烧录监视均可用） |
| 代码托管 | GitHub Public |

不用作 HomeKit 主控：正点原子 ATK-T5（涂鸦生态）。

### 开锁通道一览

| 通道 | 说明 |
|------|------|
| 实体 IC 卡 | MFRC522 白名单（课设基础/提高） |
| iPhone · Home App | 软件 HomeKit 门锁 |
| iPhone · NFC | 碰墙贴 → 快捷指令（**非**原生 Home Key） |
| 安卓 / 鸿蒙 · NFC | **手机模拟卡 → PN532 直读开锁**（如刷 IC 卡） |
| 原生 Apple Home Key | **超范围**，需 MFi，课设不做 |

详细需求见 [`docs/需求说明.md`](docs/需求说明.md)，物料见 [`docs/元器件清单.md`](docs/元器件清单.md)。

交互方案应用（手机可打开）：[`index.html`](index.html)

## 开发环境（Windows / Mac）

本仓库在 **Windows 与 Mac** 之间切换开发，工具链统一为 VS Code + PlatformIO。

### 通用步骤

1. 安装 [VS Code](https://code.visualstudio.com/) 与 PlatformIO 插件
2. 安装并登录 GitHub CLI：`gh auth login`（协议选 HTTPS）
3. 同步代码：

```bash
git clone <本仓库地址>
cd <仓库目录>
```

4. 编译：

```bash
pio run
```

5. 烧录 / 监视（端口因机器而异，勿写死进仓库）：

```bash
pio run -t upload
pio device monitor
```

### 协作约定

- 开工先 `git pull`
- 改完 `commit` 后 `push`
- `upload_port` / `monitor_port` 各机器本地处理，不提交
- 仓库默认 **Public**

### 端口差异

| 系统 | 串口示例 |
|------|----------|
| Windows | `COM3` |
| macOS | `/dev/cu.usbmodem*` / `/dev/cu.usbserial*` |

## 目录结构

```text
├── README.md
├── index.html        # 方案 A 交互应用（手机预览）
├── platformio.ini    # PlatformIO 工程配置
├── .gitignore
├── .gitattributes
├── src/              # 固件源码（PlatformIO）
├── include/          # 头文件
├── docs/             # 需求、BOM、计划、原理图说明
├── hardware/         # 原理图 / PCB（KiCad、Gerber、DRC）
├── fab/              # 打样生产文件（Gerber / BOM / 坐标）
└── assets/           # 文档配图
```

## 开发进度

| 阶段 | 内容 | 状态 |
|------|------|------|
| 0 | 环境与仓库 | 进行中 |
| 1 | 需求与方案（方案 A + BOM） | **完成** |
| 2 | 读卡模块联调 | 未开始 |
| 3 | 白名单鉴权与控门 | 未开始 |
| 4 | 管理与记录 | 未开始 |
| 5 | HomeKit / NFC 自动化 | 未开始 |
| 6 | 文档与答辩 | 未开始 |

## 组员

（待补充）
