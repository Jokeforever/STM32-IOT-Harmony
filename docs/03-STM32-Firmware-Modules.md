# STM32 端代码功能讲解

## 1. 工程概况

当前 STM32 工程路径：

```text
E:\fuchuang\code\IOT5-smart2-IOT-final
```

主要特点：

- MCU：STM32F103C8T6
- 开发方式：Keil MDK + STM32 HAL
- 当前主程序是裸机主循环 + 中断方式，没有强制依赖 FreeRTOS
- 通信方式：ESP8266 + MQTT
- 云平台：OneNET 旧版物模型

## 2. 引脚与设备对应关系

| 引脚 | 功能 | 说明 |
|---|---|---|
| PA0 | DHT11 | 温湿度传感器 |
| PA1 | 土壤湿度 | ADC1_IN1 |
| PA2 / PA3 | USART2 | ESP8266 通信 |
| PA4 | 按键 | 页面/功能键 |
| PA5 | 按键 | 页面/功能键 |
| PA6 | 按键 | 水泵模式键 |
| PA7 | TIM3_CH2 | LED PWM 输出 |
| PA9 / PA10 | USART1 | 调试串口 |
| PB0 | 光照传感器 | ADC1_IN8 |
| PB1 | 水泵 | 继电器/驱动控制 |
| PB7 | 按键 | LED 模式键 |
| PB8 / PB9 | OLED | I2C1 SCL/SDA |
| PB10 / PB11 | USART3 | CO2 传感器 |
| PB12 | 蜂鸣器 | 报警输出 |
| PC13 | 板载 LED | 状态指示 |

注意：

- USART1 主要用于调试打印，115200
- USART2 连接 ESP8266，115200
- USART3 连接 CO2 传感器，9600
- OLED 使用 PB8/PB9

## 3. 主程序结构

核心文件：

```text
Core/Src/main.c
```

主程序主要流程：

1. HAL 初始化
2. 时钟、GPIO、ADC、I2C、USART、TIM 初始化
3. `Hardware_Init()` 初始化 DHT11、OLED 等
4. `ESP8266_Init()` 初始化 ESP8266 并连接 WiFi
5. 连接 OneNET MQTT
6. `OneNET_Subscribe()` 订阅属性 set 主题
7. 主循环：
   - 按键扫描
   - 每秒读取传感器
   - `OneNet_SendData()` 上报属性
   - `OneNet_RevPro()` 处理云端下发
   - `Bump_Control()` 水泵控制
   - `Monitor_Temp()` 温度监测
   - `LED_Func()` LED 控制
   - `IS_Normal()` 异常报警
   - `Display_Data()` OLED 显示

## 4. 传感器与数据采集模块

### 4.1 DHT11 温湿度

文件：

```text
Core/Src/dht11.c
Core/Inc/dht11.h
```

作用：

- 读取空气温度和湿度
- 输出到 `temp`、`humi`

引脚：

```text
PA0
```

### 4.2 土壤湿度

文件：

```text
Core/Src/TS.c
Core/Inc/TS.h
```

作用：

- 通过 ADC 读取土壤湿度
- 输出到 `moist`

引脚：

```text
PA1 / ADC1_IN1
```

### 4.3 光照强度

文件：

```text
Core/Src/LDR.c
Core/Inc/LDR.h
```

作用：

- 通过 ADC 读取光敏电阻电压
- 转换为 `light` 光照强度

引脚：

```text
PB0 / ADC1_IN8
```

### 4.4 CO2 传感器

文件：

```text
Core/Src/usart3.c
Core/Inc/usart3.h
```

作用：

- 通过 USART3 接收 CO2 传感器数据
- 按 6 字节协议解析
- 输出到 `ppm`

引脚：

```text
PB10 / PB11
```

### 4.5 水量

由系统根据水泵流量、灌溉时间等参数计算，输出到 `water_vol`。

## 5. OLED 显示模块

文件：

```text
Core/Src/oled.c
Core/Inc/oled.h
Core/Src/Display.c
Core/Inc/Display.h
```

作用：

- OLED 初始化
- 显示字符串、数字、中文
- 分页面显示环境数据、参数、状态

引脚：

```text
PB8 / PB9
```

显示页面：

| 页面 | 内容 |
|---|---|
| 页面 1 | 空气温度、空气湿度、CO2 |
| 页面 2 | 光照强度、LED 状态 |
| 页面 3 | 土壤湿度、水量、水泵状态 |
| 页面 4 | 参数 1 |
| 页面 5 | 参数 2 |

## 6. LED 控制模块

文件：

```text
Core/Src/LED.c
Core/Inc/LED.h
```

作用：

- 通过 TIM3_CH2 PWM 控制 LED 亮度
- 支持模式：
  - `0` 关闭
  - `1` 常亮/标准模式
  - `2` 自动 PID 模式

输出引脚：

```text
PA7 / TIM3_CH2
```

自动模式逻辑：

- 读取当前光照 `light`
- 与目标光照比较
- 计算误差
- 用比例控制调整 PWM
- 死区 ±20 lux，防止振荡

## 7. 水泵控制模块

文件：

```text
Core/Src/bump.c
Core/Inc/bump.h
```

作用：

- 控制水泵开关
- 支持模式：
  - `0` 关闭
  - `1` 标准/校准模式
  - `2` 自动灌溉模式

输出引脚：

```text
PB1
```

主要变量：

```c
CurrentBump_mode
LastBump_mode
Bump_threhold[2]
```

主要函数：

| 函数 | 作用 |
|---|---|
| `BUMP_Init()` | 初始化水泵控制 |
| `Bump_Control()` | 根据当前模式控制水泵 |
| `Bump_Display()` | OLED 显示水泵状态 |
| `Bump_Set(mode)` | 切换水泵模式 |

自动灌溉逻辑：

- 根据土壤湿度阈值 `Bump_threhold[0]` 和 `Bump_threhold[1]`
- 低于下限进行灌溉
- 高于上限停止灌溉
- 还包含高压脉冲灌溉逻辑

## 8. 按键模块

文件：

```text
Core/Src/Key.c
Core/Inc/Key.h
```

4 个按键：

| 按键 | 引脚 | 作用 |
|---|---|---|
| KEY1 | PA4 | 页面切换 |
| KEY2 | PA5 | 页面切换 |
| KEY3 | PB7 | LED 模式 |
| KEY4 | PA6 | 水泵模式 |

支持：

- 短按
- 长按
- 参数增减
- 页面切换

## 9. 蜂鸣器与异常报警

文件：

```text
Core/Src/beep.c
Core/Inc/beep.h
```

作用：

- 异常状态报警
- 蜂鸣器输出

引脚：

```text
PB12
```

异常类型：

- 光照过亮
- 光照过暗
- 土壤过湿
- 土壤干旱
- CO2 过高

## 10. ESP8266 通信模块

文件：

```text
onenet/device/src/esp8266.c
onenet/device/inc/esp8266.h
```

作用：

- 通过 AT 指令初始化 ESP8266
- 连接手机热点
- 建立 TCP 连接
- 发送和接收 MQTT 数据

主要函数：

| 函数 | 作用 |
|---|---|
| `ESP8266_Init()` | ESP8266 初始化 |
| `ESP8266_SendCmd()` | 发送 AT 指令并等待响应 |
| `ESP8266_SendData()` | 发送数据 |
| `ESP8266_GetIPD()` | 读取云端下发数据 |
| `ESP8266_Clear()` | 清空接收缓存 |

关键要求：

- 手机热点必须是 2.4GHz
- 加密方式建议 WPA2
- ESP8266 供电要足够，建议独立 3.3V 电源

## 11. OneNET 通信模块

文件：

```text
onenet/onenet/src/onenet.c
onenet/onenet/inc/onenet.h
```

作用：

- OneNET 设备鉴权
- MQTT 连接
- 订阅属性 set 主题
- 上报物模型属性
- 处理云端属性下发
- 回复 set_reply

主要流程：

### 11.1 设备连接

```c
OneNet_DevLink()
```

连接参数：

- 产品 ID：`PROID`
- 设备名：`DEVICE_NAME`
- 设备密钥：`ACCESS_KEY`
- MQTT 地址：`mqtts.heclouds.com:1883`

### 11.2 属性上报

主题：

```text
$sys/{PROID}/{DEVICE_NAME}/thing/property/post
```

报文结构：

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
    "bump": { "value": 2 }
  }
}
```

注意：

- 必须包含 `version`
- 缺少 `version` 时，OneNET 可能不把数据当成物模型属性

### 11.3 属性下发

订阅主题：

```text
$sys/{PROID}/{DEVICE_NAME}/thing/property/set
```

收到：

- `led` → `LED_Set()`
- `bump` → `Bump_Set()`

回复主题：

```text
$sys/{PROID}/{DEVICE_NAME}/thing/property/set_reply
```

回复：

```json
{"id":"...","code":200,"msg":"success"}
```

## 12. 当前 STM32 端完成的功能

- [x] DHT11 温湿度采集
- [x] 土壤湿度采集
- [x] 光照强度采集
- [x] CO2 采集
- [x] 水量计算
- [x] OLED 多页面显示
- [x] 按键控制
- [x] LED PWM 控制
- [x] LED 自动 PID 模式
- [x] 水泵手动/自动控制
- [x] ESP8266 WiFi 连接
- [x] OneNET MQTT 连接
- [x] OneNET 物模型属性上报
- [x] OneNET 属性下发
- [x] LED 云端控制
- [x] 水泵云端控制
- [x] set_reply 回复

## 13. 后续可优化方向

- MQTT 断线自动重连
- OneNET 属性下发回复更细分
- 传感器数据滤波
- 参数保存到 Flash
- 增加历史数据记录
- 代码注释整理
- 引入 FreeRTOS 做任务划分
