# 项目 AI 交接文档

> **2026-10-31 当前实现补充**
> 最新固件已将配置和中值滤波拆为 modules/middleware 下的 mw_config/mw_filter。
> 本轮统一了风扇输出控制，修正 LED 默认目标和 PID dt，并更正 App 的脉冲灌溉/估算补水量名称。
> 默认目标 200 仅用于新配置、迁移补值和非法值回退，已有有效 EEPROM 目标不强制覆盖。
> 当前验证结果与烧录待办请查 CHANGELOG.md，原始真实配置工程路径不变。

最后更新：2026-11-07

## 最新验证状态（2026-11-07，PID分离）

- 独立mw_pid和LED适配分离完成，用户确认本轮五项检查全部通过。
- 验收覆盖启动数据、模式切换、自动调光方向、反复切换后的响应、掉电保存及持续运行。
- PID独立差分/边界36925项、配置控制294项、采集9030项主机断言通过。
- 真实Keil完整编译0错误5个既有cJSON警告；Code=48598、RO=6530、RW=204、ZI=7948。
- 实测确认后同步仓库脱敏副本；App源码、配置格式、OneNET属性及网络凭据未改。
- 泵/风扇/继电器实体负载测试仍暂缓；当前灯带与光敏的光学耦合、实际PID整定仍待硬件条件改善。
- 用户已授权本轮PID提交并上传Git；运行配置8e9b5cb、传感器模型040afe2已上传。

## 上一阶段验证记录（2026-11-07，运行配置）

- RuntimeConfig和统一修改入口已完成，用户确认本轮五项检查全部通过。
- 验收包含旧参数保留、App阈值修改、本机按键与边界、LED模式及掉电保存、持续运行。
- 11月1日真实Keil完整编译0错误、5个既有cJSON警告；配置/控制284项、采集9030项主机断言通过。
- 原Key.c的3个枚举混用警告消除；Code=48446、RO=6530、RW=204、ZI=7948。
- 11月7日确认后同步仓库脱敏副本；App源码、OneNET属性和EEPROM v2/48字节布局不变。
- 泵/风扇/继电器实体负载实测继续暂缓，本轮确认不扩大为这三类负载已通过。
- 运行配置已随8e9b5cb上传；传感器模型已随040afe2上传。

## 上一阶段验证记录（2026-11-01，传感器模型）

- 统一传感器数据模型与采集服务已完成，用户确认文档中的五项验收测试全部通过。
- 五项包括数据链路、光照/LED、DHT11失效恢复、App控制与掉电保存、本机温度阈值下边界。
- 采集差分测试1000组输入、9030项断言通过；控制/PID/配置/按键回归40项通过。
- 真实Keil工程完整编译0错误、8个既有警告；Code=48198、RO=6498、RW=188、ZI=7964。
- 已按先真实工程、后用户确认的流程同步仓库脱敏副本，保留真实配置与仓库占位符。
- 水泵、风扇、继电器实体测试继续暂缓；本轮五项通过不代表这三项已完成负载验证。
- App源码未修改，协议与EEPROM格式不变；传感器模型040afe2随后已上传Git。

## 上一阶段验证记录（2026-10-31）

- 用户反馈：除水泵、风扇、继电器相关部分外，其余 STM32 改动基本正常。
- 水泵、风扇、继电器：由于测试条件暂不方便而未测，先搁置；不应标记为故障，也不应写成全部实测通过。
- 主机回归：实际控制/PID/配置函数配合 GPIO、时钟和 EEPROM 模拟，32 项断言通过；修改前备份能重现风扇请求覆盖。
- Keil 完整重编译：0 错误、8 个现有警告（Key.c 3 个、cJSON 5 个）。
- App ArkTS 源码及资源编译通过，保留服务层 3 条现有异常处理警告。
- 新界面已预览；用户确认长截图中的重复、断层属于截图拼接，实际页面正常。
- 用户说明：截图采集时STM32未上电；截图仅用于界面展示，不据此新增联网故障记录。

## 修改、验证与同步约定

1. STM32 先修改并编译 `E:\fuchuang\code\IOT5-smart2-IOT-final`。
2. 等用户确认效果，再把功能变更同步到本仓库的 `stm32-firmware` 脱敏副本。
3. App 直接修改 `E:\harmony\HarmonyEmbeddedLab`。
4. 是否提交和上传 Git 由用户明确决定，验证正常不等于自动授权上传。
5. 真实 WiFi、OneNET 设备密钥和 App 用户密钥不得进入仓库。
6. 源码保持 UTF-8；未明确要求时不生成 HAP，不自动烧录、不清空 EEPROM。

## 本轮接手重点

- App新界面见docs/07，主题和公共组件分别位于theme/AppTheme.ets、components/MonitorComponents.ets。
- 控制/PID改动见docs/06和docs/09；最新36925项PID、294项控制、9030项采集断言不能替代暂缓的执行器负载实测。
- 当前模块包含app_sensors、app_config、mw_config、mw_filter、mw_pid；mw_pid不依赖HAL，LED适配负责硬件输出。
- F407/S3新底板未启用；传感器模型、运行配置和PID/LED分离均已验收；其余运行状态和平台适配待做。详见docs/08、09、10。
- 040afe2与8e9b5cb已上传；本轮PID分离验收后已同步，用户已授权提交并上传。用户说“提交”按提交并上传一起完成。

## 0. 这份文档怎么用

这份文档是给接手的 AI 或开发者快速进入项目用的。

建议阅读顺序：

1. 本文档
2. `docs/03-STM32-Firmware-Modules.md`
3. `docs/01-HarmonyOS-App-Architecture.md`
4. `docs/02-HarmonyOS-App-Debugging-Notes.md`
5. `docs/04-Project-Progress-Summary.md`

不要在没有阅读现有代码和文档的情况下直接大改工程。

---

## 1. 项目一句话概括

这是一个完整的物联网环境监控系统：

```text
STM32 采集环境数据
  ↓
ESP8266 通过 WiFi 上传 OneNET
  ↓
OneNET MQTT 物模型保存数据
  ↓
HarmonyOS App 显示数据并下发控制
```

当前阶段：

```text
核心链路与功能此前已打通，近期完成控制修复和App美化
其余STM32改动用户反馈基本正常；水泵/风扇/继电器暂未测
截图采集时STM32未上电，不据此新增联网故障记录
```

当前没有使用 RTOS，仍然是主循环 + 定时器 + 中断结构。

---

## 2. 快速事实

| 项目 | 当前内容 |
|---|---|
| 主控 | STM32F103C8T6 |
| 网络模块 | ESP8266 |
| 云平台 | OneNET 旧版 MQTT 物模型 |
| App | HarmonyOS ArkTS |
| STM32 IDE | Keil MDK 5.06 |
| App IDE | DevEco Studio 6.0.2 |
| App SDK | 6.0.2(22) |
| 参数存储 | 外部 24C02 EEPROM |
| App 版本 | v13 |
| STM32 配置版本 | v2 |
| 当前分支 | `main` |

---

## 3. 两个工程路径

### GitHub 仓库和脱敏副本

```text
E:\harmony\HarmonyEmbeddedLab
```

远程：

```text
https://github.com/Jokeforever/STM32-IOT-Harmony
```

这个工程里的 WiFi、OneNET 密钥等是脱敏占位符，不能直接烧录做真实联网测试。

### 真实硬件工程

```text
E:\fuchuang\code\IOT5-smart2-IOT-final
```

这个工程包含真实 WiFi 和 OneNET 配置，用于实际烧录和硬件测试。

原则：

```text
STM32：先改真实硬件工程，用户确认效果后再同步仓库脱敏副本
App：直接修改 E:\harmony\HarmonyEmbeddedLab
Git：仅在用户明确要求后提交、上传
真实烧录：使用 E:\fuchuang\code\IOT5-smart2-IOT-final
GitHub 提交：只提交 E:\harmony\HarmonyEmbeddedLab
```

不要把真实 WiFi 密码、AccessKey、author_key 提交到 GitHub。

---

## 4. 硬件和引脚

### 主要硬件

| 硬件 | 作用 |
|---|---|
| STM32F103C8T6 | 主控 |
| ESP8266 | WiFi、TCP/MQTT |
| DHT11 | 空气温湿度 |
| 电阻式土壤传感器 | 土壤湿度 |
| LDR 光敏模块 | 光照 |
| JW01 | 串口输出CO2相关读数 |
| OLED | 本地显示 |
| 24C02 | 参数掉电保存 |
| LED 灯带 | 照明/PID 控制 |
| 水泵 | 灌溉 |
| 风扇/继电器 | 通风/报警 |
| 蜂鸣器 | 异常报警 |
| 4 个按键 | 页面和参数调节 |

### 引脚表

| 引脚 | 功能 |
|---|---|
| PA0 | DHT11 |
| PA1 | ADC1_IN1，土壤湿度 |
| PA2 | USART2_TX，ESP8266 RX |
| PA3 | USART2_RX，ESP8266 TX |
| PA4 | 按键 1 |
| PA5 | 按键 2 |
| PA6 | 水泵模式键 |
| PA7 | TIM3_CH2，LED PWM |
| PA9 | USART1_TX，调试串口 |
| PA10 | USART1_RX，调试串口 |
| PB0 | ADC1_IN8，光敏 |
| PB1 | 水泵控制 |
| PB7 | LED 模式键 |
| PB8 | I2C1_SCL，OLED/24C02 |
| PB9 | I2C1_SDA，OLED/24C02 |
| PB10 | USART3_TX，CO2 |
| PB11 | USART3_RX，CO2 |
| PB12 | 蜂鸣器 |
| PC13 | 板载 LED |

### I2C 地址

```text
OLED   0x3C（HAL 8 位地址 0x78）
24C02  0x50（HAL 8 位地址 0xA0）
```

---

## 5. 软件结构

### STM32

目录：

```text
stm32-firmware/
  Core/Src/
  Core/Inc/
  onenet/
  Drivers/
  MDK-ARM/
```

关键文件：

| 文件 | 作用 |
|---|---|
| `main.c` | 主流程、初始化、5秒上报、配置调用、LED控制调度 |
| `modules/middleware/src/mw_config.c` | 配置、双槽CRC、迁移、延时保存 |
| `modules/middleware/src/mw_filter.c` | 三点中值滤波 |
| `adcx.c` | ADC 校准、读取、滤波 |
| `TS.c` | 土壤湿度 |
| `LDR.c` | 光照 |
| `dht11.c` | DHT11 |
| `usart3.c` | JW01 CO2 |
| `i2c.c` | I2C1、24C02 驱动 |
| `LED.c` | LED 模式、位置式 PID |
| `bump.c` | 水泵模式、自动灌溉 |
| `Key.c` | 按键、阈值调节 |
| `Display.c` | OLED 页面 |
| `onenet.c` | OneNET MQTT、属性上报/下发 |
| `esp8266.c` | ESP8266 AT 初始化、发送、接收 |

### HarmonyOS App

关键文件：

| 文件 | 作用 |
|---|---|
| `entry/src/main/ets/pages/Index.ets` | 首页、自动刷新、设备卡片、离线告警 |
| `entry/src/main/ets/pages/DeviceDetail.ets` | 详情页、图表、阈值控制、传感器状态 |
| `entry/src/main/ets/service/OneNetApiService.ets` | OneNET HTTP 鉴权、查询、下发 |
| `entry/src/main/ets/model/DeviceModels.ets` | 数据模型、属性显示和状态解析 |
| `entry/src/main/ets/config/OneNetConfig.ets` | OneNET 配置，不要泄露真实密钥 |

---

## 6. 当前已实现功能

### STM32

- [x] DHT11 温湿度
- [x] 土壤湿度
- [x] 光照
- [x] JW01 CO2
- [x] 水量估算
- [x] ADC 校准和错误检查
- [x] DWT 微秒延时
- [x] TIM4 1 MHz 毫秒延时
- [x] 传感器滤波和抗突变
- [x] DHT11 返回值区分成功/无响应/校验失败
- [x] CO2 固定字节和校验和检查
- [x] 24C02 双槽配置、CRC32、v1->v2 迁移
- [x] LED 位置式 PID
- [x] 首次上电水泵默认关闭
- [x] ESP8266 断线重连
- [x] IWDG 看门狗
- [x] `sensor_status` 上报

### HarmonyOS App

- [x] OneNET 旧版 API 鉴权
- [x] 首页在线/离线状态
- [x] 自动刷新和手动刷新
- [x] 详情页属性显示
- [x] LED 模式控制
- [x] 水泵模式控制
- [x] 四个阈值远程配置
- [x] 温湿度历史曲线
- [x] 传感器状态卡片
- [x] 离线告警横幅
- [x] App 启动恢复上次数据缓存

---

## 7. 当前重要设计说明

### 传感器有效性

`sensor_status` 只判断能可靠确认的传感器：

```text
bit0：温度
bit1：湿度
bit3：CO2
```

当前正常值通常是：

```text
11 = 0b00001011
```

光照、土壤湿度和水量仍然正常采集和显示，但不参与有效性判断，因为模拟 ADC 即使拔掉传感器也可能读到数值。

### LED PID

当前是位置式 PID：

```text
PWM = Kp * error + Ki * integral + Kd * derivative
```

默认：

```text
Kp = 1.5
Ki = 0
Kd = 0
```

默认参数是为了保持原有 P 控制效果。真正调 PID 前要先确认 LED 能对光敏传感器产生足够影响。

### 参数保存

24C02 使用双槽：

```text
Slot A = 0x00
Slot B = 0x40
```

配置带 magic、version、size、sequence 和 CRC32。  
旧版 v1 配置会自动迁移到 v2。  
参数变化后延时约 1 秒再写入，避免频繁写 EEPROM。

---

## 8. OneNET 物模型

当前使用的主要属性：

| identifier | 类型 | 读写 | 说明 |
|---|---|---|---|
| `temp` | int32 | 只读 | 温度 |
| `humi` | int32 | 只读 | 湿度 |
| `light` | int32 | 只读 | 光照 |
| `ppm` | int32 | 只读 | CO2 |
| `moist` | int32 | 只读 | 土壤湿度 |
| `water_vol` | float | 只读 | 估算水量 |
| `led` | enum | 读写 | 0/1/2 |
| `bump` | enum | 读写 | 0/1/2，模式1为脉冲灌溉 |
| `CO2threhold` | int32 | 读写 | 350~4000 |
| `droughtthrehold` | int32 | 读写 | 0~100 |
| `moistthrehold` | int32 | 读写 | 0~100 |
| `tempthrehold` | int32 | 读写 | 0~100 |
| `sensor_status` | int32 | 只读 | 传感器有效性位图 |

注意标识符拼写就是 `threhold`，不是 `threshold`。不要随意改名，否则 App、STM32 和 OneNET 会不一致。

---

## 9. 构建和烧录

### STM32

使用真实硬件工程：

```text
E:\fuchuang\code\IOT5-smart2-IOT-final\MDK-ARM\IOT.uvprojx
```

Keil 编译后下载。最新固件：

```text
E:\fuchuang\code\IOT5-smart2-IOT-final\MDK-ARM\IOT\IOT.hex
```

注意：

```text
不要把 E:\harmony\HarmonyEmbeddedLab\stm32-firmware 直接烧录到真实硬件
```

因为仓库副本的 WiFi 和 OneNET 配置是脱敏占位符。

### HarmonyOS App

使用 DevEco Studio 打开：

```text
E:\harmony\HarmonyEmbeddedLab
```

流程：

```text
Sync
  -> Build
  -> Run
```

App 真机/模拟器需要能访问 OneNET API。

---

## 10. 当前已知限制

1. LED 灯带在普通室内距离下对 LDR 影响很小，PID 无法解决光学耦合问题。
2. 光照、土壤、水量不做有效性判断，只正常显示数值。
3. PID 默认 `Ki=0`、`Kd=0`，尚未做实际参数整定。
4. JW01 是 TVOC 换算的模拟 CO2，不是真正 NDIR CO2。
5. App 目前只支持一个设备。
6. 没有 OTA。
7. 没有使用 RTOS。
8. OneNET 使用旧版 MQTT/API，后续迁移需要单独评估。

---

## 11. Git 状态和最近提交

当前分支：

```text
main
```

最近提交：

```text
c5b858c refactor: extract filter module from main.c
301e5e7 refactor: extract config module from main.c
77f3888 fix: sync LED target_lux with hardware project (500 -> 200)
d581c35 chore: convert STM32 sources to UTF-8
fa8f185 docs: add module overview comments to STM32 firmware
9645a2f docs: document app state restore cache
91b11ec feat: restore app state from local cache
9c24f44 docs: document offline warning banners
459ed14 feat: add offline warning banners
0ebd417 docs: clarify sensor status scope
5fe8afe fix: limit sensor validity to DHT11 and CO2
```

接手前先执行：

```text
git status
git log -5 --oneline
```

不要在有未确认改动时直接 reset 或覆盖文件。

---

## 12. 给接手 AI 的工作规则

1. 先读本文档和 `docs/03`，再动 STM32 代码。
2. 先读 `docs/01`，再动 App 代码。
3. 不要修改 OneNET 属性标识符，除非同步修改 STM32、App、OneNET 模型和文档。
4. 不要提交真实 WiFi 密码、AccessKey、author_key。
5. STM32 源文件现在是 UTF-8，保持 UTF-8。
6. 修改后必须编译；App 修改后必须在 DevEco Studio 构建。
7. 一次只做一类修改，完成后更新 `CHANGELOG.md`。
8. 新增参数时优先复用 24C02 配置，不要直接写死在多处。
9. 新增控制命令必须做范围检查和白名单。
10. 不要凭猜测判断硬件接线，先看数据手册或让用户提供实测现象。

---

## 13. 推荐后续方向

按优先级：

1. 硬件升级：SHT30/AHT20、BH1750、电容式土壤、MH-Z19C
2. PID 参数远程配置
3. 多设备支持
4. OTA
5. 自定义 PCB 和外壳
6. 后期再考虑 RTOS

当前不要直接做 FreeRTOS 重构，先把阻塞延时、网络等待和共享变量整理清楚。

---

## 14. 快速检查清单

接手后按这个顺序检查：

```text
[ ] git status
[ ] git log -5
[ ] 读 docs/03
[ ] 读 docs/01
[ ] 确认当前硬件工程路径
[ ] 确认 OneNET 属性标识符
[ ] 确认没有真实密钥进入 Git
[ ] 编译 STM32
[ ] DevEco 构建 App
[ ] 只改一个模块
[ ] 实测
[ ] 更新 CHANGELOG
```
