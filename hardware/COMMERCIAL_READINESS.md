# IC 卡门禁 PCB · 可商用评估与 KiCad 审查

日期：2026-09-24  
设计源：嘉立创EDA 工程 `IC卡门禁-课设` / Board1 / PCB1  
审查副本：`hardware/ic_access_control.kicad_pcb`（Protel ASCII → KiCad 10）

## 1. 已完成（可打样验证）

| 项 | 状态 |
|----|------|
| 原理图器件/位号 | 51 封装，C1–C14、R1–R16、U1–U7… |
| 网络 | 按规格书赋到焊盘（52 网） |
| 板框 | 120×80 mm，安装孔 MH1–4（转换层补 M3） |
| 布局 | 封装外框零重叠；PN532/ESP32/MFRC522 天线分区 |
| 布线 | 双层网格布线 + 过孔；天线环；GND 铺铜 |
| 生产文件 | `fab/IC_Card_Access_Gerber.zip`、BOM、坐标 CSV |
| KiCad 导入 | 成功（51 器件 / 242 焊盘 / 走线+过孔） |
| KiCad DRC | 已跑，报告 `hardware/kicad_drc_*.json` |

## 2. KiCad 审查结论（必须处理才能量产）

基线 DRC（导出即审查，未删线）：

- **948** 条违规，**31** 处未连接
- 主要：`shorting_items`、`clearance`、`solder_mask_bridge`、`silk_*`、`hole_clearance`

自动「删短路线」后：短路下降，但未连接升到 **121**——说明走线在细缝处既不满足间距也未可靠连通。

**结论：当前板可做工程打样/课设验收，尚不满足可商用量产的 DRC 门禁。**

## 3. 可商用还缺什么

### 硬件设计（回嘉立创或 KiCad 精修）

1. **清 DRC**：短路/间距以交互式推挤为准，目标 shorting=0、clearance=0（或仅丝印 warning）
2. **MFRC522 晶振**：缺 Y1 27.12 MHz + 负载电容（裸片必需）
3. **接插件**：电池 JST-PH、门锁/继电器 2P 端子、可选 SWD/UART 测试点
4. **天线匹配**：C10=15pF 占位，按 NXP AN1444/AN1445 实测定值
5. **U3 PN532 脚序**：按模组丝印最终确认
6. **阻容值复核**：1k 限流、10k 上拉、5.1k CC 与 BOM 一致
7. **SMT 经济性**：加 3 个光学定位点（fiducial）、拼板工艺边
8. **丝印**：消除丝印压铜/重叠，补极性与版本号、厂标、认证预留区

### 产品化（商用销售）

- 固件/配网/钥匙管理、防拆与安全（默认密钥、防重放）
- 结构外壳、门侧安装、EMC/ESD
- 电气安全与无线合规（如需销售：CE/FCC/SRRC 等）
- 小批量试产：SMT 钢网、测试夹具、老化与出厂检

## 4. 建议工作流

1. **嘉立创**：交互清 DRC + 补晶振/端子/fiducial（主设计）
2. **再导出** Altium/Gerber → `hardware/protel2kicad.py` → KiCad DRC 复查
3. DRC 达标后再下 SMT 订单

## 5. 文件索引

| 路径 | 说明 |
|------|------|
| `fab/IC_Card_Access_Gerber.zip` | 打样 Gerber |
| `fab/IC_Card_Access_BOM.csv` | 物料清单 |
| `fab/IC_Card_Access_PnP.csv` | 贴片坐标 |
| `hardware/ic_access_control.kicad_pcb` | KiCad 审查板 |
| `hardware/kicad_drc_baseline.json` | 导出后原始 DRC |
| `hardware/kicad_drc_final.json` | 自动删短路后 DRC |
| `hardware/protel2kicad.py` | Protel ASCII → KiCad |
| `hardware/kicad_polish.py` | 按 DRC 删短路铜 |
| `hardware/altium_export/PCB1.pcbdoc` | 嘉立创导出源 |
