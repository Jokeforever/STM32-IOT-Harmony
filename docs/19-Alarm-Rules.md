# 报警规则独立：数据、判断与提示输出

日期：2026-11-10。状态：十套主机回归和Keil完整编译通过，用户于2026-11-10反馈本轮检查均通过，已验收并同步脱敏副本。

本轮先在E:\fuchuang\code\IOT5-smart2-IOT-final完成修改、编译和用户验证，确认后同步已验证内容至仓库stm32-firmware脱敏副本。App源码、WiFi实现、真实配置与执行器控制保持原样，未自动烧录或生成HAP。

## 1. 本轮解决的职责依赖

原beep.c同时做环境超限判断和GPIO提示输出，而Abnormal报警枚举定义在Display.h中。因此不画屏幕的报警判断仍需包含显示头。

本轮新增mw_alarm独立规则，把传感器读数和五项阈值作为输入，返回光照、土壤、CO2三项状态。beep.c保留当前服务/配置适配以及提示输出，Display.h通过包含新契约继续提供原枚举名称。

```text
SensorData + 当前运行配置
        ↓ beep.c中的IS_Normal适配
SensorData + AlarmThresholds
        ↓ mw_alarm.c中的Alarm_Evaluate
三项Abnormal结果
        ↓ 复制至原abnormal[3]
原Handler → PC13光照提示、PB12土壤蜂鸣器
```

风扇仍由Monitor_Temp合并有效温度和CO2请求控制。新规则不接管风扇、继电器、水泵或LED PID，也不增加App或云端报警属性。

## 2. 文件职责

| 路径 | 职责/改动 |
|---|---|
| modules/middleware/inc/mw_alarm.h | 独立状态枚举、通道顺序、阈值结构及接口 |
| modules/middleware/src/mw_alarm.c | 无状态的严格阈值比较和CO2有效性判断 |
| Core/Src/beep.c | IS_Normal读取数据/配置、调用规则、更新原状态并调用原Handler |
| Core/Inc/Display.h | 不再拥有Abnormal定义，包含mw_alarm.h保持既有使用方式 |
| MDK-ARM/IOT.uvprojx | 将mw_alarm.c加入真实目标，沿用原middleware包含路径 |
| tests/fixtures/alarm_before_extract.c | 冻结旧beep.c判断/输出快照，仅主机对照，清理行尾空白 |
| tests/run_alarm_regressions.py | 真实规则/完整适配与旧判断/输出对比，含独立边界预期 |
| tests/run_control_regressions.py | 使用真实新枚举和规则源，保留原313项集成检查 |
| tests/run_display_regressions.py | 补新契约头，继续验证旧显示输出 |

Beep_Init和Handler函数原文保持不变。原main.c中的volatile abnormal[3]定义、主循环调用位置、GPIO初始化及其他控制文件保持不变。删除beep.c里未使用的CurrentScreen外部声明，因而无需再为显示页面类型包含Display.h。

## 3. 报警状态与通道

原类型名Abnormal和编号保留：NONE=0、BRIGHT=1、DARK=2、DAMP=3、DROUGHT=4、HIGHCO2=5。

| 数组下标 | 通道 | 可产生的状态 |
|---|---|---|
| ALARM_LIGHT_INDEX=0 | 光照 | BRIGHT、DARK、NONE |
| ALARM_SOIL_INDEX=1 | 土壤 | DAMP、DROUGHT、NONE |
| ALARM_CO2_INDEX=2 | CO2 | HIGHCO2、NONE |

ALARM_COUNT=3，原abnormal[3]仍按该顺序使用。这里的枚举及数组下标与OneNET的sensor_status位图是不同概念；sensor_status仍沿用温度bit0、湿度bit1、CO2 bit3，未改变物模型。

Display.h包含新头，因此原来通过Display.h得到Abnormal的main、显示和按键相关代码仍能编译。公共枚举不再归显示模块所有，但Display.h的其他旧硬件依赖本轮未清理。

## 4. 输入、输出与返回值

```c
typedef struct {
    uint16_t light_low;      /* lux */
    uint16_t light_high;     /* lux */
    uint16_t soil_low;       /* % */
    uint16_t soil_high;      /* % */
    uint16_t co2_threshold;  /* ppm */
} AlarmThresholds;

uint8_t Alarm_Evaluate(const SensorData *data,
                       const AlarmThresholds *thresholds,
                       Abnormal out[ALARM_COUNT]);
```

返回1表示计算完成，三项输出全部写入；任一空指针返回0，不修改输出。调用者必须提供至少三个元素。函数不分配内存，不保存历史，也不访问运行配置、HAL或OLED。

阈值结构使用与原读数/配置兼容的整数单位。范围和上下限关系仍由既有配置入口负责，规则不新增钳制或范围拒绝。即使主机边界测试故意给出倒置上下限，也保留原先检查上限、再检查下限的分支优先级。

## 5. 规则保持情况

- 光照：大于light_high为BRIGHT，小于light_low为DARK，其余NONE。
- 土壤：大于soil_high为DAMP，小于soil_low为DROUGHT，其余NONE。
- CO2：仅在SENSOR_VALID_CO2有效且大于co2_threshold时为HIGHCO2，其余NONE。

在合法上下限下，恰好等于阈值不超限。光照与土壤仍使用已有读数，不增加有效位或拔插判断。CO2失效时即使保留的最近ppm很高，也会清除CO2超限状态；温湿度有效位不能代替CO2有效位。

规则使用最近一次SensorData，不主动采集传感器。因此真实环境变化仍需等下一次采集，采样、刷新、上报频率不变。本轮没有增加回差、延迟确认、静音、蜂鸣节奏或报警锁存。

## 6. IS_Normal与原提示输出

IS_Normal先获取SensorData和当前RuntimeConfig，只读取一次配置指针，把原ldr_low/ldr_high/soil_low/soil_high/co2_threshold映射至局部AlarmThresholds。规则成功后按0、1、2顺序更新原volatile数组，再调用Handler一次。

原来逐条件多次调用AppConfig_Get，现在集中映射当前配置。阈值仍由主循环修改，现有中断只通过既有入口修改泵模式，不修改这些阈值；本轮没有引入并发配置更新机制。

Handler保留原文与输出顺序：先按光照状态写PC13，异常为低、正常为高；再按土壤状态写PB12，异常蜂鸣器开启、正常关闭。Beep_Init仍只关闭蜂鸣器，没有额外改动提示灯初始化。

CO2超限并不在本Handler中直接触发蜂鸣器。风扇由原Monitor_Temp处理，本轮保持此分工。因此不能用“CO2高时有没有蜂鸣”验证本轮CO2规则。

beep.c仍通过原beep.h间接依赖部分硬件头，输出GPIO尚未做端口提取。只有mw_alarm规则及契约具备无HAL/显示依赖的边界，不宣称整个报警服务已完成平台独立。

## 7. 回归证据

改前原控制313项及显示33379项通过。改后新回归编译完整生产beep.c、mw_alarm.c及真实公共头，同时编译冻结旧beep.c并重命名旧状态/函数。GPIO边界模拟，比较旧判断、新适配和直接规则的三项结果，以及两个GPIO调用的端口、引脚、电平和顺序。

150组独立边界向量来自光照5种位置、土壤5种位置、CO2阈值下/等于/上三种情况及两种有效性，包含多项异常组合。另有uint16端点、等上下限及倒置上下限优先级、4096组变化输入、空指针输出保护、216组提示状态组合和初始化输出对照。

共4249组新旧判断对照、108854项断言通过。断言数含逐通道逐输出重复检查，不等于108854种独立场景。输入/配置摘要也检查不变，测试中没有提供网络、EEPROM写入或执行器驱动实现。

真实规则源及公开头在只有SensorData/标准类型头的环境单独编译，公开头也通过C++语法检查；规则没有用模拟AppConfig函数替代其直接依赖，因为它根本不读取AppConfig。

十套主机回归全部通过：

- 新报警：4249组/108854项，另含150组独立边界和216组提示输出状态组合。
- 原控制：313项。
- 显示：1608组/33379项。
- LED端口：4096步/123616项。
- 按键端口：6117步/214535项。
- 传感器端口：4625项。
- 原采集：1000组/9030项。
- 命令/上报：1128项。
- 配置：1000次新旧轨迹及边界。
- PID：4096步及边界/36925项。

真实Keil完整重编译0错误、5条既有cJSON警告。Code=50306、RO-data=6614、RW-data=208、ZI-data=7952；代码段相对上一阶段增加24字节，其余列出的段大小不变。阈值/结果为局部变量，使用原有栈，段大小相同不等于已测量栈峰值。

上述是主机模拟和目标编译结果，下一节另列用户实物验收反馈；不把全部模拟边界扩大为均已硬件实测。冻结快照和测试不加入固件目标，不连接真实网络或实体负载。

## 8. 用户验收记录与原五项清单

用户于2026-11-10反馈“检查均通过”，据此记录本轮清单实物验收通过。确认后同步已验证报警规则、适配/类型调整、Keil条目、快照与测试至仓库脱敏副本，更新实际反馈并按约定提交上传。App、WiFi与真实配置保留；不扩大为所有模拟边界、可选调试观察或水泵/风扇/继电器负载均已实测。

用户未逐项补充接入硬件及可选Keil观察情况，本记录保留其实际反馈，不额外推断。原验证清单如下：

真实工程E:\fuchuang\code\IOT5-smart2-IOT-final\MDK-ARM\IOT.uvprojx，固件MDK-ARM/IOT/IOT.hex。保持当前App、硬件及配置，烧录后整板断电再上电。调整参数时记录原值，测试后恢复需要的配置。

1. **启动与同步**：联网、菜单/OLED、传感器及App读数/有效状态正常。
2. **光照与已有提示**：遮挡/照亮光敏或用本机调整光照上下限，等采集后观察区间内外的PC13现有指示灯提示与改前一致，LED自动调光也正常。
3. **土壤与阈值**：用本机/App阈值或测量条件使土壤读数进出区间；已接蜂鸣器应按原规则响/停。未接蜂鸣器则明确记录声音未实测，不要求为此新增硬件。
4. **CO2链路**：读数、有效状态及阈值设置正常。有运行调试条件时可观察abnormal[2]：有效且超限为5，其余0；CO2蜂鸣不是本轮预期，也无需运行风扇实体负载。
5. **控制与保存**：LED模式、页面/参数操作、持续刷新正常，修改参数等待几秒后重启仍保留，App同步正常。

未接提示灯/蜂鸣器时如实反馈对应未实测项；主机输出对照不能代替实体提示测试。水泵、风扇、继电器负载仍暂缓。WiFi仅保留原观察记录，本轮不做联网诊断或修改。

## 9. 后续复用与当前阶段

换主控时可继续使用数据/阈值和独立报警规则，随后适配硬件输出。将来让其他屏幕或App显示异常，可读取统一状态，但仍需单独设计显示/上报接口，本轮没有增加云端报警属性。

本轮用户已反馈检查均通过，已同步脱敏副本并更新实际验收记录，按约定完成Git提交上传并核对远程。归档后再讲下一轮计划。
