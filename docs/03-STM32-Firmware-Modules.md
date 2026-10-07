# STM32 端代码功能讲解（完整版）

## 0. 这份文档怎么用

本文不是“引脚初始化手册”，而是从功能角度讲解 STM32 端每个模块：

- 这个模块做什么
- 数据从哪里来
- 数据怎么处理
- 数据怎么上传到 OneNET
- 云端命令怎么下发到执行器
- 代码里关键函数和变量是什么

建议配合以下目录阅读：

```text
E:\fuchuang\code\IOT5-smart2-IOT-final
```

## 1. 工程概况

### 1.1 硬件

| 项目 | 内容 |
|---|---|
| MCU | STM32F103C8T6 |
| 开发工具 | Keil MDK |
| 驱动库 | STM32 HAL |
| 网络模块 | ESP8266 |
| 云平台 | OneNET 旧版物模型 |
| 显示 | OLED |
| 传感器 | DHT11、土壤湿度、光敏、CO2 |
| 执行器 | LED、水泵 |

### 1.2 软件结构

当前主程序是：

- 裸机主循环
- 中断接收串口
- 没有强制使用 FreeRTOS

主循环负责：

- 采集传感器
- 控制 LED
- 控制水泵
- 显示 OLED
- 上传 OneNET
- 处理云端下发

### 1.3 主要目录

```text
IOT5-smart2-IOT-final
├── Core/
│   ├── Inc/
│   └── Src/
├── Drivers/
├── MDK-ARM/
└── onenet/
    ├── device/
    ├── MQTT/
    └── onenet/
```

## 2. 引脚与设备对应关系

### 2.1 完整引脚表

| 引脚 | 外设/功能 | 说明 |
|---|---|---|
| PA0 | DHT11 | 温湿度传感器 |
| PA1 | ADC1_IN1 | 土壤湿度传感器 |
| PA2 | USART2_TX | ESP8266 RX |
| PA3 | USART2_RX | ESP8266 TX |
| PA4 | GPIO_Input | 按键 1 |
| PA5 | GPIO_Input | 按键 2 |
| PA6 | GPIO_Input | 水泵模式键 |
| PA7 | TIM3_CH2 | LED PWM |
| PA9 | USART1_TX | 调试串口 TX |
| PA10 | USART1_RX | 调试串口 RX |
| PB0 | ADC1_IN8 | 光敏传感器 |
| PB1 | GPIO_Output | 水泵控制 |
| PB7 | GPIO_Input | LED 模式键 |
| PB8 | I2C1_SCL | OLED / 24C02 SCL |
| PB9 | I2C1_SDA | OLED / 24C02 SDA |
| PB10 | USART3_TX | CO2 传感器 |
| PB11 | USART3_RX | CO2 传感器 |
| PB12 | GPIO_Output | 蜂鸣器 |
| PC13 | GPIO_Output | 板载 LED |

### 2.2 串口分配

| 串口 | 引脚 | 波特率 | 用途 |
|---|---|---|---|
| USART1 | PA9/PA10 | 115200 | 调试打印 |
| USART2 | PA2/PA3 | 115200 | ESP8266 |
| USART3 | PB10/PB11 | 9600 | CO2 传感器 |

### 2.3 特别注意

- ESP8266 只能使用 2.4GHz WiFi
- ESP8266 供电要足够，建议独立 3.3V 电源
- USART1 调试口接 USB-TTL 时要交叉连接 TX/RX
- OLED 使用 PB8/PB9

## 3. 主程序 `main.c`

### 3.1 初始化流程

主程序启动后依次完成：

1. `HAL_Init()`
2. `SystemClock_Config()`
3. `MX_GPIO_Init()`
4. `MX_ADC1_Init()`
5. `MX_I2C1_Init()`
6. `MX_USART1_UART_Init()`
7. `MX_USART2_UART_Init()`
8. `MX_TIM4_Init()`
9. `MX_TIM1_Init()`
10. `MX_USART3_UART_Init()`
11. `MX_TIM3_Init()`
12. `Hardware_Init()`
13. 开启串口中断接收
14. `ESP8266_Init()`
15. 连接 OneNET MQTT
16. 订阅属性 set 主题
17. 进入主循环

### 3.2 `Hardware_Init()`

主要初始化：

- DHT11
- OLED
- 其他硬件

如果 DHT11 初始化失败，会循环显示错误。

### 3.3 主循环

主循环大致做这些事：

```c
while (1)
{
    Key_Func();

    if (++timeCount >= 100)
    {
        TS_GetData(&moist);
        CO2GetData(&ppm);
        DHT11_Read_Data(&temp, &humi);
        LDR_LuxData(&light);
        OneNet_SendData();
        timeCount = 0;
        ESP8266_Clear();
    }

    dataPtr = ESP8266_GetIPD(0);
    if (dataPtr != NULL)
        OneNet_RevPro(dataPtr);

    Bump_Control();
    Monitor_Temp();
    LED_Func();
    IS_Normal();
    Display_Data();

    delay_ms(10);
}
```

时间关系：

- 主循环约 10ms 一次
- `timeCount >= 100` 约等于 1 秒
- 所以传感器和 OneNET 上报频率约为 1Hz

### 3.4 全局数据

主要全局变量：

| 变量 | 含义 |
|---|---|
| `temp` | 空气温度 |
| `humi` | 空气湿度 |
| `light` | 光照强度 |
| `ppm` | CO2 浓度 |
| `moist` | 土壤湿度 |
| `water_vol` | 水量 |
| `CurrentBump_mode` | 当前水泵模式 |
| `current_mode` | 当前 LED 模式 |
| `CO2_Threhold` | CO2 阈值 |
| `Bump_threhold[2]` | 土壤湿度上下限 |
| `temp_threhold` | 温度阈值 |

## 4. 传感器模块

## 4.1 DHT11 温湿度

文件：

```text
Core/Src/dht11.c
Core/Inc/dht11.h
```

作用：

- 单总线读取空气温度
- 单总线读取空气湿度

引脚：

```text
PA0
```

主要函数：

| 函数 | 作用 |
|---|---|
| `DHT11_Init()` | 初始化 DHT11 |
| `DHT11_Read_Data()` | 读取温度和湿度 |
| `DHT11_Read_Byte()` | 读取一个字节 |
| `DHT11_Read_Bit()` | 读取一位 |
| `DHT11_Check()` | 检测 DHT11 |
| `DHT11_Rst()` | 复位 DHT11 |

数据：

```c
DHT11_Read_Data(&temp, &humi);
```

## 4.2 土壤湿度

文件：

```text
Core/Src/TS.c
Core/Inc/TS.h
```

作用：

- 通过 ADC 读取土壤湿度传感器
- 多次采样求平均
- 输出湿度百分比

引脚：

```text
PA1
ADC1_IN1
```

主要函数：

| 函数 | 作用 |
|---|---|
| `TS_Init()` | 初始化 |
| `TS_GetData(&moist)` | 读取土壤湿度 |

每次读取：

```c
TS_GetData(&moist);
```

## 4.3 光照强度

文件：

```text
Core/Src/LDR.c
Core/Inc/LDR.h
```

作用：

- ADC 读取光敏电阻电压
- 转换为 lux 光照强度

引脚：

```text
PB0
ADC1_IN8
```

主要函数：

| 函数 | 作用 |
|---|---|
| `LDR_Init()` | 初始化 |
| `LDR_Average_Data()` | 多次采样求平均 |
| `LDR_LuxData(&light)` | 转换成光照强度 |

每次读取：

```c
LDR_LuxData(&light);
```

## 4.4 CO2 传感器

文件：

```text
Core/Src/usart3.c
Core/Inc/usart3.h
```

作用：

- USART3 接收 CO2 传感器数据
- 6 字节数据包解析
- 校验和验证
- 输出 CO2 浓度

引脚：

```text
PB10 / PB11
波特率：9600
```

数据包大小：

```c
#define USART3_RX_PACKET_SIZE 6
```

主要函数：

| 函数 | 作用 |
|---|---|
| `USART3_Rx_Start_IT()` | 启动中断接收 |
| `CO2GetData(&ppm)` | 获取 CO2 数据 |

每次读取：

```c
CO2GetData(&ppm);
```

## 4.5 水量计算

水量 `water_vol` 不是单独传感器直接读数，而是根据：

- 水泵流量
- 灌溉时间
- 土壤湿度
- 目标补水量

估算得到。

相关宏：

```c
#define AREA                0.08f
#define WATER_DEPTH         10.0f
#define WATER_DENSITY       1.0f
#define FLOW_RATE           1.2f
#define MIN_WATER_VOLUME    0.1f
```

## 5. OLED 显示模块

文件：

```text
Core/Src/oled.c
Core/Inc/oled.h
Core/Src/Display.c
Core/Inc/Display.h
```

OLED 接口：

```text
PB8 / PB9
```

主要显示内容：

| 页面 | 内容 |
|---|---|
| 页面 1 | 空气温度、空气湿度、CO2 |
| 页面 2 | 光照强度、LED 状态 |
| 页面 3 | 土壤湿度、水量、水泵状态 |
| 页面 4 | 参数 1 |
| 页面 5 | 参数 2 |

显示函数：

| 函数 | 作用 |
|---|---|
| `Display_Data()` | 主显示调度 |
| `Refresh1_Data()` | 页面 1 刷新 |
| `Refresh2_Data()` | 页面 2 刷新 |
| `Refresh3_Data()` | 页面 3 刷新 |
| `Refresh4_Data()` | 页面 4 刷新 |
| `Refresh5_Data()` | 页面 5 刷新 |
| `Bump_Display()` | 水泵状态 |
| `LED_Display()` | LED 状态 |

## 6. LED 控制模块

文件：

```text
Core/Src/LED.c
Core/Inc/LED.h
```

### 6.1 硬件

```text
PA7 / TIM3_CH2
```

通过 PWM 控制亮度。

### 6.2 模式

```c
typedef enum {
    MODE_OFF = 0,
    MODE_CALIBRATION = 1,
    MODE_AUTO_PID = 2,
} System_Mode;
```

对应 App：

```text
0 → 关闭
1 → 常亮
2 → 自动
```

### 6.3 主要函数

| 函数 | 作用 |
|---|---|
| `LED_Init()` | 初始化 PWM |
| `LED_Set(mode)` | 设置 LED 模式 |
| `LED_Update(current_lux)` | PID/比例控制更新 |
| `LED_SetPWM(pwm)` | 设置 PWM |
| `LED_Func()` | 主循环中执行 |
| `LED_Display()` | OLED 显示 |

### 6.4 自动模式

自动模式逻辑：

1. 读取当前光照 `light`
2. 目标光照 `target_lux`
3. 误差：

```text
error = target_lux - current_lux
```

4. 死区：

```text
±20 lux
```

5. 比例调整：

```text
adjust = error * k_p
```

6. 限制每次调整量：

```text
±50
```

7. 限制 PWM 范围：

```text
0 ~ 1000
```

### 6.5 关键参数

```c
target_lux = 500
k_p = 1.5f
dead_zone = 20
initial_pwm = 300
PWM_MAX = 1000
```

## 7. 水泵控制模块

文件：

```text
Core/Src/bump.c
Core/Inc/bump.h
```

### 7.1 硬件

```text
PB1
```

通过 GPIO 控制继电器/驱动。

### 7.2 模式

```c
typedef enum {
    Bump_OFF = 0,
    Bump_CALIBRATION = 1,
    Bump_AUTO = 2,
} Bump_Mode;
```

对应 App：

```text
0 → 关闭
1 → 开启/标准
2 → 自动灌溉
```

### 7.3 主要函数

| 函数 | 作用 |
|---|---|
| `BUMP_Init()` | 初始化 |
| `Bump_Control()` | 根据模式控制水泵 |
| `Bump_Display()` | OLED 显示 |
| `Bump_Set(mode)` | 切换模式 |
| `Irrigation_Control()` | 自动灌溉逻辑 |

### 7.4 自动灌溉

阈值：

```c
Bump_threhold[0] // 干旱下限
Bump_threhold[1] // 湿润上限
```

默认：

```c
HUMI_THRESHOLD_LOW  30
HUMI_THRESHOLD_HIGH 75
```

逻辑：

- 土壤湿度低于下限 → 开始灌溉
- 土壤湿度高于上限 → 停止灌溉
- 中间区域 → 保持当前状态

### 7.5 水量估算

参数：

```c
AREA = 0.08f
WATER_DEPTH = 10.0f
FLOW_RATE = 1.2f
MIN_WATER_VOLUME = 0.1f
```

可根据面积、目标水深和流量估算灌溉时间。

### 7.6 高压脉冲灌溉

代码中还有：

- `hp_running`
- `hp_ms_total`
- `hp_ms_count`
- `hp_is_on`
- `HP_ON_MS`
- `HP_OFF_MS`

用于间歇式/高压脉冲灌溉。

## 8. 按键模块

文件：

```text
Core/Src/Key.c
Core/Inc/Key.h
```

4 个按键：

| 按键 | 引脚 | 作用 |
|---|---|---|
| 按键 1 | PA4 | 页面切换 |
| 按键 2 | PA5 | 页面切换 |
| 按键 3 | PB7 | LED 模式 |
| 按键 4 | PA6 | 水泵模式 |

支持：

- 短按
- 长按
- 参数增减

参数步进：

```c
LDR_STEP 100
CO2_STEP 100
TEMP_STEP 2
BUMP_STEP 2
```

## 9. 蜂鸣器与异常报警

文件：

```text
Core/Src/beep.c
Core/Inc/beep.h
```

引脚：

```text
PB12
```

异常类型：

```c
typedef enum {
    NONE = 0,
    BRIGHT,
    DARK,
    DAMP,
    DROUGHT,
    HIGHCO2
} Abnormal;
```

对应：

- 光照过亮
- 光照过暗
- 土壤过湿
- 土壤干旱
- CO2 过高

主要函数：

| 函数 | 作用 |
|---|---|
| `IS_Normal()` | 判断异常 |
| `Handler()` | 报警处理 |
| `Beep_Set(status)` | 蜂鸣器开关 |

## 10. ESP8266 通信模块

文件：

```text
onenet/device/src/esp8266.c
onenet/device/inc/esp8266.h
```

### 10.1 作用

- AT 指令控制 ESP8266
- 连接 2.4GHz WiFi
- TCP 连接 OneNET MQTT
- 发送 MQTT 数据
- 接收 OneNET 下发数据

### 10.2 串口

```text
USART2
PA2 / PA3
115200
```

### 10.3 初始化步骤

```c
AT
AT+CWMODE=1
AT+CWDHCP=1,1
AT+CWJAP="SSID","PASSWORD"
AT+CIPSTART="TCP","mqtts.heclouds.com",1883
```

### 10.4 主要函数

| 函数 | 作用 |
|---|---|
| `ESP8266_Init()` | 初始化 WiFi |
| `ESP8266_Clear()` | 清空接收缓存 |
| `ESP8266_WaitRecive()` | 判断接收完成 |
| `ESP8266_SendCmd()` | 发送 AT 指令并检查响应 |
| `ESP8266_SendData()` | 发送 MQTT 数据 |
| `ESP8266_GetIPD()` | 读取云端下发数据 |

### 10.5 关键注意

- 热点必须 2.4GHz
- WPA2 最稳
- 不支持的 5GHz 对 ESP8266 等于不存在
- 供电不足会导致 CWJAP 阶段掉电复位
- 串口接收首字节丢失会导致 AT 响应匹配失败

## 11. OneNET 通信模块

文件：

```text
onenet/onenet/src/onenet.c
onenet/onenet/inc/onenet.h
```

### 11.1 产品与设备

工程中定义：

```c
#define PROID       "OCuh518nh5"
#define DEVICE_NAME "SA1"
#define ACCESS_KEY  "设备级密钥"
```

MQTT 地址：

```text
mqtts.heclouds.com:1883
```

### 11.2 鉴权

旧版 MQTT 鉴权版本：

```text
2018-10-31
```

资源格式：

```text
products/{PROID}/devices/{DEVICE_NAME}
```

### 11.3 主要函数

| 函数 | 作用 |
|---|---|
| `OneNET_Authorization()` | 生成 MQTT 鉴权 |
| `OneNet_DevLink()` | 连接 OneNET |
| `OneNet_FillBuf()` | 组装属性上报 JSON |
| `OneNet_SendData()` | 上传属性 |
| `OneNET_Publish()` | 发布 MQTT 消息 |
| `OneNET_Subscribe()` | 订阅 set 主题 |
| `OneNet_RevPro()` | 处理云端下发 |

### 11.4 属性上报

主题：

```text
$sys/{PROID}/{DEVICE_NAME}/thing/property/post
```

报文：

```json
{
  "id": "123",
  "version": "1.0",
  "params": {
    "temp": { "value": 26 },
    "humi": { "value": 40 },
    "light": { "value": 207 },
    "ppm": { "value": 350 },
    "water_vol": { "value": 0.0 },
    "led": { "value": 2 },
    "bump": { "value": 2 },
    "CO2threhold": { "value": 2000 },
    "droughtthrehold": { "value": 30 },
    "moistthrehold": { "value": 75 },
    "tempthrehold": { "value": 30 },
    "moist": { "value": 97 }
  }
}
```

注意：

- 必须有 `version`
- 必须是 OneJSON 结构
- OneNET 才会把它当成物模型属性

### 11.5 属性下发

订阅：

```text
$sys/{PROID}/{DEVICE_NAME}/thing/property/set
```

收到：

```json
{
  "id": "xxx",
  "version": "1.0",
  "params": {
    "led": 2
  }
}
```

或：

```json
{
  "id": "xxx",
  "version": "1.0",
  "params": {
    "bump": 2
  }
}
```

处理：

- `led` → `LED_Set()`
- `bump` → `Bump_Set()`

### 11.6 set_reply

回复主题：

```text
$sys/{PROID}/{DEVICE_NAME}/thing/property/set_reply
```

回复：

```json
{"id":"xxx","code":200,"msg":"success"}
```

## 12. MQTT 协议层

文件：

```text
onenet/MQTT/MqttKit.c
onenet/MQTT/MqttKit.h
```

作用：

- MQTT 连接报文
- MQTT 发布报文
- MQTT 订阅报文
- MQTT 解包
- QoS 处理

主要函数：

| 函数 | 作用 |
|---|---|
| `MQTT_PacketConnect()` | 连接报文 |
| `MQTT_PacketPublish()` | 发布报文 |
| `MQTT_PacketSubscribe()` | 订阅报文 |
| `MQTT_PacketSaveData()` | 保存属性上报数据 |
| `MQTT_UnPacketRecv()` | 判断报文类型 |
| `MQTT_UnPacketPublish()` | 解析发布报文 |
| `MQTT_UnPacketSubscribe()` | 解析订阅响应 |

## 13. 调试串口

文件：

```text
Core/Src/myusart.c
Core/Inc/myusart.h
```

USART1：

```text
PA9 / PA10
115200
```

用于：

- 打印 ESP8266 AT 响应
- 打印调试信息
- 观察 CWJAP 返回值

常见返回：

| 返回 | 含义 |
|---|---|
| `+CWJAP:1` | 找不到热点 |
| `+CWJAP:2` | 密码错误 |
| `+CWJAP:3` | 连接超时 |
| `+CWJAP:4` | 加密方式不支持 |

## 14. OneNET 属性模型对照表

| identifier | 中文 | 类型 | 读写 | 单位 | 说明 |
|---|---|---|---|---|---|
| `temp` | 空气温度 | int32 | 只读 | ℃ | DHT11 |
| `humi` | 空气湿度 | int32 | 只读 | % | DHT11 |
| `light` | 光照强度 | int32 | 只读 | lx | 光敏 |
| `ppm` | CO2 浓度 | int32 | 只读 | ppm | USART3 |
| `moist` | 土壤湿度 | int32 | 只读 | % | ADC |
| `water_vol` | 水量 | float | 只读 | 无 | 估算 |
| `led` | LED 模式 | enum | 读写 | 无 | 0/1/2 |
| `bump` | 水泵模式 | enum | 读写 | 无 | 0/1/2 |
| `CO2threhold` | CO2 阈值 | int32 | 读写 | ppm | 参数 |
| `droughtthrehold` | 干旱阈值 | int32 | 读写 | % | 参数 |
| `moistthrehold` | 湿润阈值 | int32 | 读写 | % | 参数 |
| `tempthrehold` | 温度阈值 | int32 | 读写 | ℃ | 参数 |

## 15. 控制链路

### 15.1 LED

```text
App 下发 led=0/1/2
  ↓
OneNET set 主题
  ↓
OneNet_RevPro()
  ↓
LED_Set(mode)
  ↓
LED_Func()
  ↓
LED_SetPWM()
```

### 15.2 水泵

```text
App 下发 bump=0/1/2
  ↓
OneNET set 主题
  ↓
OneNet_RevPro()
  ↓
Bump_Set(mode)
  ↓
Bump_Control()
  ↓
BUMP_ON / BUMP_OFF
```

## 16. 常见问题与解决

### 16.1 OLED 卡在 CWJAP

检查：

- 热点是否为 2.4GHz
- 密码是否正确
- ESP8266 供电是否足够
- AT 指令是否收到响应

### 16.2 AT 响应丢失

检查：

- `main.c` 是否使用 `HAL_UART_Receive_IT(&huart2, &received_data, 1)`
- `myusart.c` 是否把 `received_data` 追加到 `esp8266_buf`
- `esp8266_cnt` 类型是否为 `unsigned short`

### 16.3 物模型没有值

检查：

- 属性上报 JSON 是否有 `version`
- 主题是否是 `thing/property/post`
- OneNET 物模型是否有对应属性

### 16.4 水泵控制无效

检查：

- `onenet.c` 的 `bump` 分支是否调用 `Bump_Set()`
- `bump.h` 是否声明 `Bump_Set()`
- STM32 是否收到 set 消息
- 是否发送 set_reply

### 16.5 断电后 App 仍在线

检查：

- App 是否使用属性 `time` 判断在线
- 是否以 30 秒作为离线阈值

## 17. 如何扩展新功能

### 17.1 增加新传感器

1. 编写传感器驱动
2. 在主循环读取
3. 在 `OneNet_FillBuf()` 增加属性
4. OneNET 物模型增加属性
5. App `PropertyHelper` 增加中文名和单位
6. 详情页增加卡片

### 17.2 增加新控制

1. OneNET 物模型增加可写属性
2. `OneNet_RevPro()` 增加解析
3. 编写控制函数
4. 发送 set_reply
5. 下一次上报新状态
6. App 增加模式按钮

### 17.3 增加参数保存

当前已经采用外部 24C02 EEPROM，具体实现见 17.4。

### 17.4 24C02 参数掉电保存实现

当前工程已经在 `i2c.c/i2c.h` 中增加了 24C02 硬件 I2C 驱动，复用 OLED 使用的 I2C1：

```text
PB8 -> SCL
PB9 -> SDA
```

24C02 使用 7 位地址 `0x50`，HAL 使用 8 位地址 `0xA0`。

配置结构：

```c
typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint16_t sequence;
    uint8_t led_mode;
    uint8_t bump_mode;
    uint8_t led_manual_pwm;
    uint8_t temp_threshold;
    uint16_t co2_threshold;
    uint16_t ldr_low;
    uint16_t ldr_high;
    uint16_t soil_low;
    uint16_t soil_high;
    uint16_t soil_wet_adc;
    uint16_t soil_dry_adc;
    uint32_t crc32;
} DeviceConfig;
```

存储策略：

1. Slot A 地址 `0x00`
2. Slot B 地址 `0x40`
3. 每次写空闲槽
4. 写完后读回
5. CRC32 正确才切换活动槽
6. 两个槽都无效时加载默认配置
7. 参数修改后延时 1 秒再写，避免连续按键频繁写 EEPROM

首次无有效配置时：

- 使用默认阈值
- LED 默认自动 PID
- 水泵默认 `Bump_OFF`
- 1 秒后自动写入默认配置

当前已经完成的硬件验证：

- OLED 和 24C02 共用 I2C1 正常
- 参数写入正常
- 掉电重启后参数恢复
- LED 模式恢复
- 水泵模式恢复
- 24C02 空白时默认配置回退正常

## 18. 当前 STM32 端完整功能清单

- [x] DHT11 温湿度
- [x] 土壤湿度
- [x] 光照强度
- [x] CO2 浓度
- [x] 水量估算
- [x] OLED 多页面显示
- [x] 4 个按键
- [x] LED PWM
- [x] LED 自动 PID
- [x] 水泵手动模式
- [x] 水泵自动灌溉
- [x] 蜂鸣器报警
- [x] ESP8266 WiFi
- [x] OneNET MQTT
- [x] 物模型属性上报
- [x] 属性下发
- [x] LED 云端控制
- [x] 水泵云端控制
- [x] set_reply
- [x] ADC 校准和错误检查
- [x] DWT 微秒延时
- [x] 传感器有效性位图
- [x] 24C02 参数掉电保存

## 19. 后续优化方向

- [x] MQTT 断线重连
- [x] 参数掉电保存
- [ ] 传感器滤波
- 更精细的 PID
- 任务化/FreeRTOS
- 代码注释整理
- 多设备支持
- OTA 升级

## 20. 总结

STM32 端核心可以概括为：

```text
传感器采集
  ↓
本地控制与显示
  ↓
OneNET_FillBuf 组装 OneJSON
  ↓
MQTT 属性上报
  ↓
OneNet_RevPro 处理下发
  ↓
LED_Set / Bump_Set
  ↓
执行器动作
  ↓
下一次属性上报
```

理解这条链路，就理解了整个 STM32 端项目的核心。
