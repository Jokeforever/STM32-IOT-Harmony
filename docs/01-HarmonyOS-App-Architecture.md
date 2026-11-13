# 鸿蒙端 App 代码功能分区讲解（完整版）

## 2026-11-10 双阈值温控与本机参数页归档

默认通风上限30℃、加热下限20℃，各有1℃回差；CO2独立超限请求通风，低温与高CO2允许同时通风/加热。新增纯规则、App双阈值及第7项命令，EEPROM v3/52字节兼容v1/v2读取，旧单温度项迁移30/20，其他设置保留。
用户确认原第1、2项正常及本机补充四项通过；第二参数页光标/字体/行距统一按用户明确授权免追加实测提交。2026-11-10 归档时风扇和加热器实测暂缓。2026-11-13 用户反馈“五项测试均通过”，完成风扇和加热片实物补测。覆盖高温通风回差、低温加热回差、CO₂独立通风、同时通风加热以及温度失效处理。已同步本轮脱敏固件、App及测试/文档，按授权归档上传；真实配置和未跟踪硬件原文件保留。
最终13套固件与两套App回归通过；显示31601项/1629次页面检查、控制345项、温控97034项。Keil0错误5条既有警告，Code51078/RO6654/RW220/ZI7956；App此前ArkTS通过保留3条既有警告，本次App源码未再变，未生成HAP。
学习、当前代码与验收范围见[通风加热与本机参数页](21-Climate-Control.md)。下文待验收/未同步等表述保留为过程记录，以本节与当前源码为准。


## 2026-11-10 补充本机温度上下限（待本机验收）

用户反馈上一轮第1、2项正常；有关风扇/加热器的测试因条件不便暂缓，不记为全部实测通过。按用户追加要求，真实STM32参数页4现为LL光照下限、LH光照上限、TH通风上限、TL加热下限，带“>”选择光标，按键2循环四项，按键3/4每次增减温度1℃且保持选中。
本机共用AppConfig配对校验与v3保存，页5仍保留原土壤/CO2编号；修正跨参数页进入时的选择范围，扫描消抖/中断及温控算法不变。控制345项、显示32781项/1618次页面检查、按键214535项、温控97034项通过，真实Keil0错误5条既有警告（Code50998/RO6654/RW220/ZI7956）。
本次没有追加App源码改动；等待本机OLED/按键/同步/保存验证，负载继续暂缓，仓库固件副本未同步、未提交上传。详见[本机补充与实际验收范围](21-Climate-Control.md)。


## 2026-11-10 真实工程/App：通风与加热双阈值（待本轮实测）

按用户确认将温度控制改为独立通风上限30℃/加热下限20℃、1℃状态回差；CO2独立超限请求通风，低温与CO2超限允许双开。新增mw_climate纯规则、App下限调整及第7项命令；保留tempthrehold作为上限，新增temp_low_threshold作为下限。
EEPROM v3为52字节/CRC偏移48，可读v1/v2；旧单温度项迁移为30/20，其余原参数保留，已存v3合法设置不覆盖。13套固件主机回归和两套App回归通过；Keil0错误5条既有警告，ArkTS通过保留3条既有警告，未生成HAP。
用户已反馈原第1、2项正常；风扇/加热器相关检查暂缓。本机追加功能待验证；仓库固件副本仍为afd5720，未同步本轮固件或提交上传，真实配置保留。详见[通风加热规则与五项验收](21-Climate-Control.md)。


## 2026-11-10 报警输出端口（用户验收通过并归档）

新增mw_alarm_port契约与app_alarm_port当前板卡适配，将PC13提示灯/PB12蜂鸣器的低有效电平集中映射。Handler仅替换两处提示灯写入；BEEP_ON/OFF兼容宏转调蜂鸣器端口。IS_Normal、Beep_Init函数原文、报警规则、GPIO启动配置和其他控制保留。
新输出端口3653项、原报警4249组/108854项及其余九套回归通过；真实Keil完整编译0错误5条既有cJSON警告，Code=50298、RO=6614、RW=208、ZI=7952。
用户于2026-11-10反馈“验证均通过”，据此记录本轮五项清单实物验收通过。确认后同步已验证报警输出端口、兼容宏、Keil条目、旧宏快照与测试至仓库脱敏副本，更新验收记录并按约定提交上传。App、真实配置及其他控制保留；不将本轮反馈扩大为所有模拟边界或暂缓的水泵/风扇/继电器实体负载均已实测。详见[报警输出接口学习与验收](20-Alarm-Port.md)。


## 2026-11-10 报警规则独立（用户验收通过并归档）

新增mw_alarm契约/纯规则，Abnormal类型从Display.h归入独立头并保持原名/编号。IS_Normal读取数据与阈值后调用规则，原Handler和Beep_Init原文不变，保留严格超限、仅CO2有效位、提示输出与风扇分工。
新报警4249组/108854项及原九套回归通过；真实Keil完整编译0错误5条既有cJSON警告，Code=50306、RO=6614、RW=208、ZI=7952。
用户于2026-11-10反馈“检查均通过”，据此记录本轮清单实物验收通过。确认后同步已验证报警规则、适配/类型调整、Keil条目、快照与测试至仓库脱敏副本，更新实际反馈并按约定提交上传。App、WiFi与真实配置保留；不扩大为所有模拟边界、可选调试观察或水泵/风扇/继电器负载均已实测。详见[报警规则学习与验收](19-Alarm-Rules.md)。


## 2026-11-10 本机模式显示归位（五项验收通过并归档）

LED_Display与Bump_Display的实现集中到Display.c，原声明、调用入口、坐标/字库和配置读取保持原样。LED.c移除直接OLED依赖；13个控制函数、原页面代码、按键/ISR/采集/联网规则均保持原文。
新显示1608组/33379项及原八套回归通过；真实Keil完整编译0错误5条既有cJSON警告，Code=50282、RO=6614、RW=208、ZI=7952，与上一阶段段大小相同。
用户于2026-11-10明确确认本轮五项测试均通过。确认后同步已验证的三份显示/控制源码、显示快照与测试至仓库脱敏副本，更新验收记录并按约定提交上传。App源码、WiFi实现与真实联网配置保留，水泵/风扇/继电器实体负载仍暂缓。详见[显示与控制职责分离](18-Display-Separation.md)。


## 2026-11-10 按键输入接口（五项验收通过并归档）

真实工程新增mw_key_port.h与app_key_port.c，按PA4/PA5/PB7/PA6原顺序读取低有效四键，统一为bit0..3按下掩码。Key_Scan只替换输入赋值，状态机、Key_Func与TIM1回调业务保持原文；扫描节奏、消抖/长按计数及页面/控制操作不变。
新旧扫描6117次/214535项、16种映射与原控制313项及其余六套回归全部通过。真实Keil完整编译0错误5条既有cJSON警告，Code=50282、RO=6614、RW=208、ZI=7952。
用户于2026-11-10明确确认本轮五项检查均通过。确认后同步已验证按键端口、扫描输入改动、Keil条目、测试和说明至仓库脱敏副本，并按约定提交上传。App源码及真实联网配置保留；水泵/风扇/继电器实体负载仍暂缓。详见[按键接口学习与验收](17-Key-Port.md)。


## 2026-11-10 传感器读取接口（实物验收通过并归档）

真实工程新增mw_sensor_port.h与app_sensor_port.c，采集服务改用四个统一读取接口；驱动成功语义显式转换，统一1成功/0失败，失败保留输出。采集顺序、整数单位、滤波、有效位、光照/土壤拔插策略与主循环节奏不变。
新增端口4625项及原采集1000组/9030项通过，LED123616、控制313、命令1128、PID36925及配置1000次轨迹全部通过。真实Keil完整编译0错误5条既有cJSON警告；Code=50266、RO=6614、RW=208、ZI=7952。
用户于2026-11-10反馈：“我试了一下断电重启，连上了，其他功能测试后也正常。”据此记录本轮实物验收通过，随后同步已验证源码、Keil条目、测试和说明至仓库脱敏副本，并按约定提交上传。App源码与真实联网配置保留；泵/风扇/继电器实体负载继续暂缓。详见[采集接口学习与验收](16-Sensor-Port.md)。


## 2026-11-10 LED运行期端口（五项验收通过并归档）

真实工程新增mw_led_port.h与app_led_port.c，将LED运行期PWM、启停/关灯和毫秒时基收拢；公共LED.h不再包含HAL/Key/OLED。LED_Display仍保留原OLED实现，不宣称整个LED模块已完全独立。
统一0～1000千分比占空比，当前ARR=999时输出与旧实现一致；PID、模式、配置和定时器初始化保持原规则。补beep.c对Display.h的直接依赖，报警逻辑不变。
LED端口123616项断言/4096步对照、原控制313、命令1128、PID36925、采集9030和配置1000次轨迹通过；真实Keil完整编译0错误5条既有cJSON警告，Code=50082、RO=6614、RW=208、ZI=7952。
用户于2026-11-10明确确认本轮五项实物检查均通过。确认后同步已验证源码、Keil条目、回归与说明至仓库脱敏副本，更新文档并按约定提交上传；App源码及真实联网配置保留。泵/风扇/继电器实体负载仍暂缓。详见docs/15。


## 2026-11-09 命令处理层（五项验收通过并同步）

真实工程与仓库脱敏副本已包含mw_command/app_commands/onenet_command，OneNET解析与参数规则、当前硬件适配分开。覆盖原6项属性，合法同值返回成功且不重复保存/重置LED，非法批次执行前拒绝。
云端土壤上下限冲突现明确拒绝，本机按键原钳制规则保留；设备应答区分200/400/500。主循环增加有节流的事件上报，保留独立5秒采集周期。
1128项命令/真实JSON/分发与上报集成检查及原控制313、PID36925、采集9030、配置1000次轨迹通过；真实Keil完整编译0错误5条既有cJSON警告，Code=50014、RO=6610、RW=208、ZI=7952。
用户于2026-11-09确认本轮五项验证均通过，随后同步仓库脱敏副本并保留真实配置。App未改，未自动烧录，用户已授权本轮提交上传；实体负载测试仍暂缓。详见docs/14。


## 2026-11-08 控制应答与上报确认（用户认可，阶段归档）

本轮只改App。新增ControlTracker与ControlFeedback，读取OneNET外层code和data.code；设备应答、目标值、新上报匹配及超时分别记录。
模式高亮和阈值只显示最近实际查询值，不再把目标写入properties。待确认目标使用描边/提示，同属性等待期间禁止重复下发。
详情页最多临时每2秒查询，设备应答后等待新属性上报30秒；普通自动刷新关闭时仍完成控制确认。超时是结果未确认，不自动重发。
脉冲灌溉的设备应答作为一次命令反馈，不强求短暂模式1被轮询捕获，不代表实物出水量已经验证。
40场景152项控制回归（生产模型、模拟网络的实际服务及提取的实际页面方法）通过；原趋势68场景127项继续通过。ArkTS编译通过，3条既有服务警告保留。
用户认可当前控制确认版本并授权提交上传；未将本次认可扩大为全部专项边界或实体负载均已实测。未生成HAP、未修改STM32，上一阶段趋势已随bf8b349上传。详见docs/13。


## 2026-11-08 阶段归档：上报趋势与10倍缩放

用户已认可当前10倍缩放方案并明确授权提交、上传。归档包含按属性上报时间去重、时间轴、两种时间展示、缓存迁移、大图及滚轮/滑条/双指/平移；最大倍率统一为10。
68场景127项主机检查与ArkTS编译通过，保留3条既有服务警告；用户认可当前版本，不扩大为所有输入设备和边界场景均已逐项实测。
本次归档仅包含App源码、测试及文档，STM32、真实凭据、HAP和未跟踪硬件原文件不纳入。下文“未提交”“待验收”和100倍说明保留为过程历史，以本节及当前源码为准。


## 2026-11-08 连续滚轮与滑条缩放（交互待实测）

用户反馈上一版仅切换局部量程仍不够，需要滚轮式缩放。本轮在大图内加入滚轮、1～10倍连续滑条、双指捏合、拖动平移、双击/重置。
默认缩放数值轴，可选时间轴或双轴；刻度、当前倍率及可见时间窗同步更新。普通卡片不注册这些手势，保留页面滚动。
缩放轴锁定视窗，未缩放轴继续按实时历史更新；新上报仍写入同一历史，重置回到最新范围。视窗外的线段由容器裁剪，未把数值压到边界冒充读数。
新增15场景43项视窗与滚轮计算检查，总计68场景127项通过；ArkTS编译通过，保留3条既有服务警告。
滚轮事件路由、方向、双指、拖动及页面滚动仍待用户运行App验证；未生成HAP、未改STM32、未提交上传Git。详见docs/12。


按用户要求，上限现统一为10倍，由TrendHistory.MAX_ZOOM供滚轮/双指边界、滑条映射与标签共用。下文100倍内容保留为历史。

## 2026-11-08 曲线放大查看（本轮待实测）

用户反馈上一轮五项检查“应该通过”，并提供了实际曲线截图。本轮按此记录反馈，不扩大为所有边界均已实测。
温湿度曲线各增加“放大”入口，通过bindContentCover打开大图，可切换温度/湿度、局部放大/完整量程，并关闭返回原详情位置。
局部纵轴按当前样本范围缩放，带刻度和明确提示；完整量程仍为温度0～50℃、湿度0～100%。
HistoryChart已抽到components/HistoryChart.ets，普通绘图区高128，放大绘图区高280，新增纵轴刻度、网格和最新点标记。
大图直接接收原@State历史的@Prop，不复制静态数据；使用原刷新入口，未修改定时、控制、网络或STM32逻辑。
新增8场景20项缩放/高度/范围检查，总计53场景84项通过；ArkTS编译通过，保留3条既有服务警告。
放大界面的触控、返回和持续刷新仍待用户实测；本轮未生成HAP，未提交上传Git。详见docs/12的放大说明。


## 2026-11-08 App 趋势与时间（当前实现）

趋势已从 number[] 改为 TrendPoint[]，每一点包含属性自己的毫秒 time 和数值 value。
新增 model/TrendHistory.ets，将有效位检查、去重、乱序保护、20点限长、旧缓存迁移和时间坐标计算从页面抽出。
首页与详情每次成功同步后都合并最新共享历史；详情通过 @State/@Prop 更新图表，横轴按实际时间间隔绘制。
首页和详情分开展示最后同步与设备上报；前者是查询时间，后者是当前属性快照中的最大合法上报时间。
温湿度曲线分别使用 temp.time/humi.time，有效位分别为 bit0/bit1；缺失时间时跳过，不以手机时间代替。
缓存 historyVersion=2；旧无时间 number[] 丢弃，仅从可靠的缓存属性建立起点。它与固件 EEPROM v2 无关。
45场景64项主机检查与 ArkTS 编译通过，服务层3条既有警告保留；用户本轮实测待确认，未生成HAP或提交上传。
详细学习和五项实测见 [上报趋势说明](12-App-Report-Trends.md)。下文旧数组与按查询入图示例属于历史实现，以本节和当前源码为准。

## 2026-11-08 配置存储接口与App

固件将24C02和毫秒时基调用集中到ConfigPort，App源码与物模型属性未改。
用户确认本轮App保存、本机保存、连续修改保留最后值、旧配置加载及持续运行五项通过。
保存仍在最后一次有效变化后等待原1000ms，不能把界面回显直接等同于此刻已写入EEPROM。
详细固件学习和验收范围见docs/11-Configuration-Port.md。

## 2026-11-07 PID分离与App关系

本轮固件将PID计算从LED驱动提取，App源码、OneNET属性、模式值和EEPROM配置不变。
用户已确认App/本机LED模式、自动调光、保存和持续运行等本轮五项通过。
本轮没有增加App PID编辑界面，也未完成灯带光学耦合或实际PID整定；详见docs/10。

## 2026-11-07 与运行配置的对接

固件阈值与模式已经由AppConfig统一管理；App仍使用原有属性和模式值，源码未改。
云端与板载按键共享参数检查，字段改变后通知延时保存。用户已确认本轮App阈值修改、LED模式、
掉电保存和数据链路等五项验收通过。PID参数集中管理不等于增加了App PID编辑界面。
详细固件教程见docs/09-Runtime-Configuration.md。

## 2026-11-01 与固件数据模型的关系

固件内部统一为SensorData和采集服务；传给App的属性名、数值单位和有效位保持原规则，App源码无需调整。
用户已确认本轮五项验收（含数据链路、App LED控制、阈值调整和掉电保存）通过；
不据此扩大为所有执行器负载均已验证。固件学习说明见docs/08。

> **2026-10-31 当前实现补充**
> 水泵模式仍使用原协议值 0/1/2；模式 1 的准确名称改为“脉冲灌溉”，不改变设备行为。
> `water_vol` 显示为“估算补水量”，单位 L，不能用作实测水量。详情页增加脉冲运行后自动关闭的说明。

## 2026-10-31 当前页面结构

- 首页重做为环境总览、独立同步时间、刷新工具栏、速览读数、设备入口和模式摘要。
- 详情按环境读数、设备控制、阈值、趋势、采集状态、可展开的全部属性组织。
- 新增 `theme/AppTheme.ets` 与 `components/MonitorComponents.ets`，统一颜色、状态标签、卡片和提示。
- 新增 `monitor_*.svg` 本地图标；动态展示保留 @Prop，不把数值改回易产生旧显示问题的 Builder 参数。
- 阈值按键保持独立 Text 加减符号，点击区域44×44；图表按容器宽度绘制。
- 水泵模式1准确显示为“脉冲灌溉”；补水量卡片按L显示两位小数，全部属性保留原始精度。
- 无属性时显示 `--`，未取得上报时间时显示“未确认”；原在线算法未重构。
- 操作提示支持展开原始详情；截图采集时STM32未上电，界面截图不作为新联网故障记录。
- 网络、控制、缓存和刷新方法与改动前逐段比对，保持原逻辑。编译与预览范围见 docs/07。

下文保留原架构教程；涉及旧蓝色仪表盘、固定图表宽度、始终展开属性等旧布局时，以本节和当前源码为准。

## 0. 这份文档怎么用

这不是一份“只看目录”的说明，而是一份可以配合源码逐段阅读的教程。

建议阅读顺序：

1. 先看第 1 章，理解 App 在整个物联网系统中的位置。
2. 再看第 2、3 章，理解工程目录和分层设计。
3. 然后按第 4 章逐个文件阅读源码。
4. 最后看第 5～10 章，理解数据流、OneNET 协议、ArkUI 刷新规则和调试方法。

本文对应工程：

```text
E:\harmony\HarmonyEmbeddedLab
```

## 1. App 在系统中的位置

整个项目的数据链路：

```text
STM32 采集传感器数据
        ↓
ESP8266 通过 2.4GHz WiFi 连接 OneNET
        ↓
OneNET MQTT 接收属性上报
        ↓
OneNET 物模型保存最新属性
        ↓
iot-api.heclouds.com 提供旧版 HTTP API
        ↓
HarmonyOS App 查询属性 / 下发控制
```

鸿蒙 App 负责两部分：

### 1.1 读取数据

App 调用 OneNET 旧版物模型查询接口，拿到：

- `temp`：空气温度
- `humi`：空气湿度
- `light`：光照强度
- `ppm`：CO2 浓度
- `moist`：土壤湿度
- `water_vol`：水量
- `led`：LED 模式
- `bump`：水泵模式
- `CO2threhold`：CO2 阈值
- `droughtthrehold`：干旱阈值
- `moistthrehold`：湿润阈值
- `tempthrehold`：温度阈值

### 1.2 下发控制

App 通过 OneNET `set-device-property` 接口下发：

- `led = 0/1/2`
- `bump = 0/1/2`

STM32 收到 MQTT 属性下发后执行：

- `LED_Set()`
- `Bump_Set()`

## 2. 技术栈与开发环境

### 2.1 技术栈

| 项目 | 内容 |
|---|---|
| 开发工具 | DevEco Studio 6.0.2 Release |
| 语言 | ArkTS |
| UI 框架 | ArkUI 声明式 UI |
| SDK | Compatible SDK 6.0.2(22) |
| 网络 | `@kit.NetworkKit` 的 `http` |
| 编码/Base64 | `@kit.ArkTS` 的 `util` |
| 加密 | 纯 ArkTS SHA1 / HMAC-SHA1 |
| 云平台 API | OneNET 旧版 `iot-api.heclouds.com` |

### 2.2 应用信息

```text
Bundle Name: com.example.harmonyembeddedlab
Module: entry
Device Type: phone
```

### 2.3 权限

`entry/src/main/module.json5` 中声明了：

```json
{
  "name": "ohos.permission.INTERNET"
}
```

没有网络权限，App 无法访问 OneNET。

## 3. 工程目录与功能分区

### 3.1 总体目录

```text
HarmonyEmbeddedLab
├── AppScope/
│   ├── app.json5
│   └── resources/
├── entry/
│   ├── src/main/
│   │   ├── ets/
│   │   │   ├── config/
│   │   │   │   └── OneNetConfig.ets
│   │   │   ├── model/
│   │   │   │   └── DeviceModels.ets
│   │   │   ├── service/
│   │   │   │   └── OneNetApiService.ets
│   │   │   ├── pages/
│   │   │   │   ├── Index.ets
│   │   │   │   └── DeviceDetail.ets
│   │   │   ├── entryability/
│   │   │   └── entrybackupability/
│   │   ├── resources/
│   │   └── module.json5
│   └── oh-package.json5
├── docs/
├── build-profile.json5
└── hvigorfile.ts
```

### 3.2 各目录职责

| 目录/文件 | 职责 |
|---|---|
| `config/OneNetConfig.ets` | OneNET 用户 ID、author_key、产品 ID、设备名 |
| `model/DeviceModels.ets` | 数据模型、枚举、属性工具类 |
| `service/OneNetApiService.ets` | OneNET HTTP API、鉴权、属性查询、属性下发 |
| `pages/Index.ets` | 首页仪表盘、设备卡片、自动刷新 |
| `pages/DeviceDetail.ets` | 详情页、环境数据、LED/水泵控制、全部属性 |
| `entryability/EntryAbility.ets` | Ability 生命周期入口 |
| `resources/base/element` | 颜色、字符串等基础资源 |
| `module.json5` | 模块配置和权限 |

## 4. 核心文件逐一讲解

## 4.1 配置层：`OneNetConfig.ets`

作用：

保存 OneNET 旧版用户级鉴权参数。

结构：

```ets
export class OneNetConfig {
  static readonly USER_ID: string = '...';
  static readonly ACCESS_KEY: string = '...';
  static readonly PRODUCT_ID: string = '...';
  static readonly DEVICE_NAME: string = '...';
}
```

字段说明：

| 字段 | 说明 |
|---|---|
| `USER_ID` | OneNET 用户 ID，旧版鉴权资源为 `userid/{USER_ID}` |
| `ACCESS_KEY` | 用户级 author_key，不是 STM32 的设备 MQTT 密钥 |
| `PRODUCT_ID` | 当前产品 ID，例如 `OCuh518nh5` |
| `DEVICE_NAME` | 当前设备名，例如 `SA1` |

注意：

- App 使用的是 **用户级** author_key。
- STM32 使用的是 **设备级** 密钥。
- 两者不是同一个东西，不能混用。

安全：

- 当前是学习项目，密钥直接放在 App 内。
- Git 仓库中真实 `OneNetConfig.ets` 已被 `.gitignore` 忽略。
- 仓库中只提交 `OneNetConfig.sample.ets`。
- 正式产品应把 Token 生成放到服务器。

## 4.2 数据模型层：`DeviceModels.ets`

这个文件是 App 的“公共语言”，页面和服务层都依赖它。

### 4.2.1 `DeviceState`

```ets
export enum DeviceState {
  OFFLINE = 0,
  ONLINE = 1
}
```

用于表示设备状态。

### 4.2.2 `LedMode`

```ets
export enum LedMode {
  OFF = 0,
  ON = 1,
  AUTO = 2
}
```

与 OneNET 里的 `led` 属性对应。

### 4.2.3 `DeviceInfo`

`DeviceInfo` 是早期设备列表模型，当前首页主要使用顶层状态字段，
但该类仍保留，方便后续扩展多设备。

核心字段：

```ets
id
name
state
temperature
humidity
properties
isRealDevice
lastSyncTime
lastOnlineTime
```

### 4.2.4 `DeviceProperty`

这是 OneNET 返回的属性项在 App 内的统一结构：

```ets
export interface DeviceProperty {
  identifier: string;
  value: string;
  name?: string;
  dataType?: string;
  accessMode?: string;
  time?: number;
}
```

字段含义：

| 字段 | 含义 |
|---|---|
| `identifier` | 属性标识，例如 `temp` |
| `value` | 属性值，统一转成字符串 |
| `name` | OneNET 返回的属性中文名 |
| `dataType` | 数据类型，例如 `int32`、`float`、`enum` |
| `accessMode` | 读写权限，例如“只读”“读写” |
| `time` | 属性上报时间戳，单位毫秒 |

`time` 非常重要，App 用它判断设备是否在线。

### 4.2.5 `DeviceCloudStatus`

```ets
export interface DeviceCloudStatus {
  online: boolean;
  lastOnlineTime: string;
}
```

用于承载设备状态查询结果。

### 4.2.6 `DeviceDetailParam`

页面跳转参数：

```ets
export interface DeviceDetailParam {
  id: number;
  name: string;
  state: DeviceState;
  temperature: number;
  humidity: number;
  properties: DeviceProperty[];
  isRealDevice: boolean;
  lastSyncTime: string;
  lastOnlineTime: string;
}
```

首页进入详情页时把当前数据带上，详情页返回时再把最新数据带回。

### 4.2.7 `PropertyHelper`

这是整个 App 最常用的工具类。

#### `getValue(properties, identifier)`

根据属性标识找值：

```ets
const temp = PropertyHelper.getValue(properties, 'temp');
```

如果没有找到，返回空字符串 `''`。

#### `getLabel(identifier)`

把英文标识转成中文：

| identifier | 中文 |
|---|---|
| `temp` | 空气温度 |
| `humi` | 空气湿度 |
| `light` | 光照强度 |
| `ppm` | CO2 浓度 |
| `moist` | 土壤湿度 |
| `water_vol` | 水量 |
| `led` | LED 模式 |
| `bump` | 水泵模式 |
| `CO2threhold` | CO2 阈值 |
| `droughtthrehold` | 干旱阈值 |
| `moistthrehold` | 湿润阈值 |
| `tempthrehold` | 温度阈值 |

#### `getUnit(identifier)`

给属性补单位：

| identifier | 单位 |
|---|---|
| `temp` | ℃ |
| `humi` | % |
| `light` | lx |
| `ppm` | ppm |
| `moist` | % |
| `droughtthrehold` | % |
| `moistthrehold` | % |
| `CO2threhold` | ppm |
| `tempthrehold` | ℃ |

#### `formatValue(identifier, value)`

把值格式化成适合显示的文本：

- 空值 → `--`
- `led` → 关闭/常亮/自动
- `bump` → 关闭/开启/自动
- 其他 → 原值

#### `getLedModeText(value)`

```text
0 → 关闭
1 → 常亮
2 → 自动
```

#### `getBumpModeText(value)`

```text
0 → 关闭
1 → 开启
2 → 自动
```

#### `isKnownProperty(identifier)`

判断是否属于当前已知属性，便于后续扩展“其他属性”分区。

## 4.3 服务层：`OneNetApiService.ets`

这个类负责所有和 OneNET 的 HTTP 交互。

### 4.3.1 接口常量

查询属性：

```text
GET https://iot-api.heclouds.com/thingmodel/query-device-property
```

下发属性：

```text
POST https://iot-api.heclouds.com/thingmodel/set-device-property
```

查询设备详情：

```text
GET https://iot-api.heclouds.com/device/detail
```

### 4.3.2 `queryDeviceProperties()`

流程：

1. `http.createHttp()`
2. `createAuthorization()` 生成鉴权头
3. 拼接 URL：

```text
?product_id=产品ID
&device_name=设备名
&_t=时间戳
```

4. `http.request()` 发起 GET
5. 检查 HTTP 状态
6. 解析 JSON
7. 检查 OneNET `code`
8. 把原始属性项转换成 `DeviceProperty[]`
9. `httpRequest.destroy()`

关键点：

- `usingCache: false`
- `_t` 时间戳
- `connectTimeout: 10000`
- `readTimeout: 10000`

### 4.3.3 `queryDeviceStatus()`

用于查询设备真实在线状态：

- 返回 `online`
- 返回 `lastOnlineTime`

虽然当前页面主要用属性时间判断在线，但这个方法保留，便于后续使用。

### 4.3.4 `setLedMode(mode)`

```ets
await this.setDeviceProperty('led', mode);
```

### 4.3.5 `setBumpMode(mode)`

```ets
await this.setDeviceProperty('bump', mode);
```

### 4.3.6 `setDeviceProperty(identifier, value)`

通用属性下发方法。

根据 `identifier` 构造：

```json
{
  "product_id": "产品ID",
  "device_name": "设备名",
  "params": {
    "led": 2
  }
}
```

或：

```json
{
  "product_id": "产品ID",
  "device_name": "设备名",
  "params": {
    "bump": 2
  }
}
```

### 4.3.7 `createAuthorization()`

OneNET 旧版用户级鉴权。

参数：

```text
version = 2022-05-01
method  = sha1
resource = userid/{USER_ID}
et = 当前时间 + 365 天
```

待签名字符串：

```text
et
sha1
userid/用户ID
2022-05-01
```

最终 Authorization：

```text
version=2022-05-01
&res=userid%2F用户ID
&et=过期时间
&method=sha1
&sign=签名
```

### 4.3.8 `hmacSha1()` 与 `sha1()`

这是纯 ArkTS 实现的 SHA1 和 HMAC-SHA1。

为什么不用系统 cryptoFramework：

- 调试时发现不同实现存在兼容差异
- 纯 ArkTS 实现可以逐字节对照 Node.js 标准结果
- 保证签名和 OneNET 期望一致

核心步骤：

1. key 超过 64 字节先 SHA1
2. key 补充到 64 字节
3. ipad = key XOR 0x36
4. opad = key XOR 0x5C
5. SHA1(ipad + message)
6. SHA1(opad + innerHash)

### 4.3.9 错误处理

`ensureHttpOk()`：

- 检查 HTTP 状态是否为 200

`ensureApiOk()`：

- 检查 OneNET `code`
- `0` 或 `"0"` 表示成功
- 其他值抛出异常，错误信息包含 code 和 msg

## 4.4 首页：`Index.ets`

首页是 App 的监控仪表盘。

### 4.4.1 组件 `DeviceCard`

这是一个带 `@Prop` 的独立组件：

```ets
@Component
struct DeviceCard {
  @Prop name: string;
  @Prop online: boolean;
  @Prop temperature: number;
  @Prop humidity: number;
  @Prop propertyCount: number;
  @Prop lastSyncTime: string;
  onOpenDetail?: () => void;
}
```

为什么用 `@Prop`：

- 它会随父组件状态变化更新
- 避免带参数 `@Builder` 不刷新的问题

卡片显示：

- 设备名
- 最后同步时间
- 在线/离线状态
- 温度
- 湿度
- 数据项数量
- 查看详情入口

### 4.4.2 组件 `StatChip`

顶部统计项：

```ets
@Component
struct StatChip {
  @Prop label: string;
  @Prop value: string;
}
```

用于：

- 在线设备
- 版本号
- 最后同步

### 4.4.3 `Index` 的状态字段

```ets
@State deviceName: string = '环境节点';
@State deviceOnline: boolean = false;
@State temperature: number = 0;
@State humidity: number = 0;
@State properties: DeviceProperty[] = [];
@State ledMode: number = -1;
@State lastSyncTime: string = '--';
@State lastOnlineTime: string = '--';
@State lastReportTime: number = 0;
@State loading: boolean = false;
@State errorMessage: string = '';
@State autoRefresh: boolean = true;
```

这些字段全部是顶层 `@State`，只要赋值，UI 就会刷新。

### 4.4.4 `aboutToAppear()`

进入首页时：

```ets
this.syncOneNet();
this.startAutoRefresh();
```

### 4.4.5 `startAutoRefresh()`

每 10 秒调用一次：

```ets
setInterval(() => {
  if (this.autoRefresh && !this.loading) {
    this.syncOneNet();
  }
}, 10000);
```

### 4.4.6 `stopAutoRefresh()`

清除定时器，防止页面离开后继续请求。

### 4.4.7 `refreshOneNetData()`

流程：

1. 查询属性
2. 遍历属性
3. 找最大 `time`
4. 更新 `properties`
5. 更新温度、湿度、LED 模式
6. 更新最后上报时间
7. 判断在线状态
8. 更新最后同步时间

在线判断：

```ets
this.deviceOnline =
  (Date.now() - this.lastReportTime) < 30000;
```

含义：

- 30 秒内有上报 → 在线
- 超过 30 秒没有上报 → 离线

### 4.4.8 `syncOneNet()`

带 loading 和错误处理的入口：

```ets
if (this.loading) return;
this.loading = true;
try {
  await this.refreshOneNetData();
} catch (error) {
  this.errorMessage = 'OneNET 同步失败：' + err.message;
} finally {
  this.loading = false;
}
```

### 4.4.9 `buildDetailResult()`

构造详情页参数：

```ets
{
  id: 1,
  name: this.deviceName,
  state: this.deviceOnline ? ONLINE : OFFLINE,
  temperature: this.temperature,
  humidity: this.humidity,
  properties: this.properties,
  isRealDevice: true,
  lastSyncTime: this.lastSyncTime,
  lastOnlineTime: this.lastOnlineTime
}
```

### 4.4.10 `openDetail()`

1. 暂停首页定时器
2. 使用 `pushPathByName` 进入详情页
3. 详情页返回时接收结果
4. 更新首页状态
5. 重新启动定时器
6. 再立即同步一次

### 4.4.11 首页 UI 结构

```text
Navigation
└── Column
    ├── 顶部蓝色仪表盘
    │   ├── 标题
    │   ├── 同步按钮
    │   └── 在线设备 / 版本号 / 最后同步
    ├── 错误提示
    ├── 自动刷新 / 立即刷新
    ├── DeviceCard
    └── Blank
```

## 4.5 详情页：`DeviceDetail.ets`

详情页是功能最完整的页面。

### 4.5.1 组件拆分

| 组件 | 作用 |
|---|---|
| `DetailHeaderInfo` | 头部小信息 |
| `DetailMetricCard` | 大数值卡片 |
| `DetailPropertyRow` | 普通属性行 |
| `DetailModeButton` | 模式按钮 |

全部使用 `@Prop`，避免 Builder 不刷新问题。

### 4.5.2 状态字段

```ets
@State deviceId
@State deviceName
@State online
@State temperature
@State humidity
@State properties
@State lastSyncTime
@State lastOnlineTime
@State lastReportTime
@State ledMode
@State bumpMode
@State loading
@State errorMessage
@State autoRefresh
```

### 4.5.3 `onReady()`

进入详情页时：

1. 保存 `navPathStack`
2. 从导航参数读取设备信息
3. 更新状态字段
4. 解析 `led`
5. 立即 `refresh()`
6. 启动 10 秒自动刷新

### 4.5.4 `refreshData()`

和首页类似，但额外处理：

- `bumpMode`
- 全部属性

流程：

1. 查询属性
2. 遍历 `properties`
3. 找 `latestTime`
4. 更新 `properties`
5. 更新温度、湿度
6. 更新 LED 模式
7. 更新水泵模式
8. 更新最后在线时间
9. 判断在线
10. 更新最后同步时间

### 4.5.5 `refresh()`

手动刷新入口：

```ets
this.loading = true;
try {
  await this.refreshData();
} finally {
  this.loading = false;
}
```

### 4.5.6 `setLedMode(mode)`

1. 调用 `oneNetApi.setLedMode(mode)`
2. 立即更新 `ledMode`
3. 再调用 `refreshData()` 从云端回读

### 4.5.7 `setBumpMode(mode)`

1. 调用 `oneNetApi.setBumpMode(mode)`
2. 立即更新 `bumpMode`
3. 再调用 `refreshData()` 从云端回读

### 4.5.8 `getDisplayValue()` 和 `getUnit()`

把属性值转成显示文本：

```ets
PropertyHelper.formatValue(
  identifier,
  PropertyHelper.getValue(this.properties, identifier)
)
```

### 4.5.9 全部属性实时刷新

使用：

```ets
ForEach(
  this.properties,
  (property) => { ... },
  (property) =>
    property.identifier + '_' +
    property.value + '_' +
    (property.time === undefined ? '0' : property.time.toString())
)
```

key 包含值和时间，所以属性变化时会重新生成行。

### 4.5.10 详情页 UI 结构

```text
NavDestination
└── Scroll
    └── Column
        ├── 设备头部
        ├── 错误提示
        ├── 自动刷新 / 刷新数据 / 返回
        ├── 环境数据
        ├── 执行器
        │   ├── LED 模式控制
        │   └── 水泵模式控制
        ├── 阈值设置
        └── 全部属性
```

## 5. 数据流详解

### 5.1 启动读取

```text
Index.aboutToAppear()
  → syncOneNet()
  → refreshOneNetData()
  → queryDeviceProperties()
  → 更新 @State
  → UI 刷新
```

### 5.2 自动刷新

```text
setInterval 10s
  → syncOneNet()
  → queryDeviceProperties()
  → 更新状态
```

### 5.3 进入详情

```text
Index.openDetail()
  → stopAutoRefresh()
  → pushPathByName(deviceDetail, param)
  → DeviceDetail.onReady()
  → refresh()
  → startAutoRefresh()
```

### 5.4 返回首页

```text
DeviceDetail 返回
  → pop(buildResult())
  → Index 回调
  → 更新首页状态
  → startAutoRefresh()
  → syncOneNet()
```

### 5.5 控制 LED

```text
DetailModeButton
  → setLedMode(mode)
  → OneNetApiService.setLedMode()
  → POST set-device-property
  → STM32 收到 led
  → LED_Set()
  → STM32 下一次上报
  → App 刷新
```

### 5.6 控制水泵

```text
DetailModeButton
  → setBumpMode(mode)
  → OneNetApiService.setBumpMode()
  → POST set-device-property
  → STM32 收到 bump
  → Bump_Set()
  → Bump_Control()
  → STM32 下一次上报
  → App 刷新
```

## 6. OneNET 属性与 App 映射

| identifier | 中文 | 单位 | 模式 |
|---|---|---|---|
| `temp` | 空气温度 | ℃ | 数值 |
| `humi` | 空气湿度 | % | 数值 |
| `light` | 光照强度 | lx | 数值 |
| `ppm` | CO2 浓度 | ppm | 数值 |
| `moist` | 土壤湿度 | % | 数值 |
| `water_vol` | 水量 | 无 | 数值 |
| `led` | LED 模式 | 无 | 0/1/2 |
| `bump` | 水泵模式 | 无 | 0/1/2 |
| `CO2threhold` | CO2 阈值 | ppm | 数值 |
| `droughtthrehold` | 干旱阈值 | % | 数值 |
| `moistthrehold` | 湿润阈值 | % | 数值 |
| `tempthrehold` | 温度阈值 | ℃ | 数值 |
| `sensor_status` | 传感器状态 | 无 | 位图 |

> 截至 2026-10-07，STM32 端新增了 24C02 EEPROM 参数掉电保存，但 App 使用的 OneNET 属性标识符没有变化。App 不需要修改就能继续读取和下发已有的 `led`、`bump` 等属性。
> 传感器读取异常时，STM32 会保留最后一次有效值；`sensor_status` 已作为 OneNET 只读属性上报。当前 App 只显示温度、湿度和 CO2 的有效状态；光照、土壤湿度和水量不做有效性判断，避免模拟传感器拔掉后出现误判。
> 2026-10-10 增加的传感器滤波只在 STM32 内部处理，不改变 OneNET 属性格式和 App 接口。
> 2026-10-11 增加的完整 PID、PID 参数保存和配置 v1->v2 迁移同样不改变 App 属性接口；只有调大 `Ki/Kd` 后，LED 自动模式的表现才会发生变化。
> 2026-10-11 起，详情页四个阈值改为可调步进控件，App 可以通过 OneNET 下发阈值，STM32 校验后保存到 24C02。
> 2026-10-12 起，详情页增加温度/湿度历史曲线，自动刷新时在 App 内存中保留最近 20 个点。
> 2026-10-13 起，详情页增加传感器状态卡片，根据 `sensor_status` 位图显示每个传感器是否有效。
> 2026-10-13 起，首页和详情页在设备离线时显示红色告警横幅，并显示最后在线时间。
> 2026-10-13 起，App 使用 `PersistentStorage + AppStorage` 缓存最近一次成功数据，启动时先显示缓存再联网刷新。

## 7. ArkUI 状态刷新规则

这是本项目中最重要的经验。

### 7.1 顶层 `@State`

推荐：

```ets
@State temperature: number = 0;

this.temperature = 25;
```

不推荐把动态数据只放在普通对象内部。

### 7.2 带参数的 `@Builder` 问题

不推荐：

```ets
@Builder
metric(label: string, value: string) { ... }
```

动态数据推荐：

```ets
@Component
struct MetricCard {
  @Prop value: string;
}
```

或者直接在父组件里内联渲染。

### 7.3 `ForEach` key

稳定数据：

```ets
(item) => item.identifier
```

实时数据：

```ets
(item) =>
  item.identifier + '_' +
  item.value + '_' +
  item.time
```

## 8. 常见错误与排查

### 8.1 10403 invalid authorization

检查：

- author_key 是否有效
- USER_ID 是否对应
- version 是否为 `2022-05-01`
- 签名是否正确

### 8.2 数据一直是 0 或 `--`

检查：

- `usingCache` 是否为 false
- URL 是否有 `_t`
- STM32 是否在上报
- OneNET 物模型是否有 value

### 8.3 在线状态不准

检查：

- 属性 `time` 是否存在
- 是否在 30 秒内
- 设备是否已断电

### 8.4 控制按钮没反应

检查：

- `loading` 是否为 false
- `isRealDevice` 是否被错误判断
- OneNET set-device-property 是否成功
- STM32 是否处理对应属性

## 9. 安全建议

- 不要把 author_key 提交到 Git
- 不要把 Token 输出到日志
- 正式版本把 Token 放到服务器
- 使用 HTTPS
- 对错误信息做脱敏

## 10. 后续扩展指南

### 10.1 新增传感器

1. OneNET 物模型增加属性
2. STM32 `OneNet_FillBuf()` 增加字段
3. `PropertyHelper.getLabel()` 增加中文名
4. `PropertyHelper.getUnit()` 增加单位
5. 详情页增加卡片

### 10.2 新增控制

1. OneNET 物模型增加可写属性
2. STM32 `OneNet_RevPro()` 增加处理分支
3. `OneNetApiService` 增加 `setXxxMode()`
4. 详情页增加控制组件
5. 保存状态并回读

### 10.3 新增页面

1. 新建 `pages/Xxx.ets`
2. 使用 `NavDestination`
3. 在首页 `deviceDetailPage` 类似的导航入口注册
4. 使用 `NavPathStack` 跳转

### 10.4 历史曲线

可以增加：

- 历史属性查询接口
- 本地数组保存最近 N 个点
- ArkUI `Line`/`Polyline` 绘制
- 时间轴和坐标轴

## 11. 总结

这个 App 的核心可以概括为：

```text
顶层 @State 保存数据
OneNetApiService 负责请求
PropertyHelper 负责显示转换
Index 负责监控
DeviceDetail 负责控制和详细展示
30 秒时间戳判断在线
10 秒定时器自动刷新
```

掌握了这套结构，就可以继续添加新的传感器、控制项、页面和图表。
