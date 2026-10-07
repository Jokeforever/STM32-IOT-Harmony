# STM32 端工程说明

这是 HarmonyOS + STM32 + OneNET 物联网项目中的 STM32 固件工程。

## 1. 目录结构

```text
stm32-firmware/
├── Core/
│   ├── Inc/
│   └── Src/
├── Drivers/
│   ├── CMSIS/
│   │   ├── Device/ST/STM32F1xx/
│   │   └── Include/
│   └── STM32F1xx_HAL_Driver/
├── MDK-ARM/
│   ├── IOT.uvprojx
│   ├── IOT.uvoptx
│   └── startup_stm32f103xb.s
├── onenet/
│   ├── CJSON/
│   ├── MQTT/
│   ├── device/
│   └── onenet/
├── IOT.ioc
└── .mxproject
```

## 2. 开发环境

- MCU：STM32F103C8T6
- 开发工具：Keil MDK
- 驱动库：STM32 HAL
- 网络模块：ESP8266
- 云平台：OneNET 旧版物模型

## 3. 打开工程

使用 Keil 打开：

```text
MDK-ARM/IOT.uvprojx
```

然后：

1. 检查器件型号是否为 STM32F103C8。
2. 检查下载器配置。
3. 全量重新编译。
4. 烧录到 STM32。

## 4. 必须填写的配置

为了不把真实密钥和 WiFi 密码上传到 GitHub，仓库中的代码使用占位符。

### 4.1 OneNET 设备密钥

文件：

```text
onenet/onenet/src/onenet.c
```

找到：

```c
#define ACCESS_KEY "YOUR_DEVICE_ACCESS_KEY"
```

替换为你 OneNET 设备的 MQTT 密钥。

注意：

- 这是设备级密钥
- 不是 App 里的用户 author_key

### 4.2 WiFi 名称和密码

文件：

```text
onenet/device/src/esp8266.c
```

找到：

```c
#define ESP8266_WIFI_INFO "AT+CWJAP=\"YOUR_WIFI_SSID\",\"YOUR_WIFI_PASSWORD\"\r\n"
```

替换为你的 2.4GHz WiFi 名称和密码。

ESP8266 要求：

- 2.4GHz
- WPA2
- 供电电流足够

## 5. 主要功能

- DHT11 温湿度采集
- 土壤湿度采集
- 光照强度采集
- CO2 采集
- 水量估算
- OLED 多页面显示
- LED PWM 控制
- LED 自动 PID
- 水泵手动/自动控制
- 4 个按键
- 蜂鸣器异常报警
- ESP8266 WiFi 连接
- OneNET MQTT 连接
- OneNET 物模型属性上报
- OneNET 属性下发
- LED/水泵云端控制
- set_reply 回复

## 6. 最近完成的优化

- `OneNet_RevPro` 增加空指针检查和 `break`
- `set_reply` 的 id 缓冲区从 14 字节扩大到 64 字节
- HMAC-SHA1 的 Base64 长度改为固定 20 字节
- 修复 author_key 解码长度计算
- 修复 `pm` 拼写残留
- `usart3` 静态变量改为全局定义
- USART2 接收变量改为 `volatile`
- ESP8266 清缓存时同步清 `cntPre`
- CWJAP 单独使用 30 秒超时
- ESP8266 初始化增加有限重试
- 主程序改为按 `HAL_GetTick()` 每 5 秒上报
- DHT11、TCP、OneNET 登录增加有限重试
- `OneNet_FillBuf` 改为带边界检查的 `snprintf`
- `ESP8266_SendData` 和 `OneNet_SendData` 增加发送状态返回
- 主循环增加 TCP/MQTT 断线重连检查
- 增加 IWDG 看门狗，超时约 8 秒
- AT 等待和网络等待循环中会喂狗，避免长等待误复位

## 7. 当前注意事项

- 这是从本地工程精简复制的版本，不包含 `MDK-ARM/IOT` 编译产物
- 第一次编译时 Keil 会自动生成对象文件和输出目录
- 如果使用了不同的 WiFi，请只使用 2.4GHz
- 如果设备密钥泄露，请在 OneNET 控制台重新生成
- 当前自动重连主要针对 TCP/MQTT；ESP8266 通常会自动重连上次保存的 AP
