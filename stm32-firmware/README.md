# STM32 端工程说明

这是 HarmonyOS + STM32 + OneNET 物联网项目中的 STM32 固件工程。

## 修改、验证与同步约定

1. STM32 先修改并编译 `E:\fuchuang\code\IOT5-smart2-IOT-final`。
2. 等用户确认效果，再把功能变更同步到本仓库的 `stm32-firmware` 脱敏副本。
3. App 直接修改 `E:\harmony\HarmonyEmbeddedLab`。
4. 是否提交和上传 Git 由用户明确决定，验证正常不等于自动授权上传。
5. 真实 WiFi、OneNET 设备密钥和 App 用户密钥不得进入仓库。
6. 源码保持 UTF-8；未明确要求时不生成 HAP，不自动烧录、不清空 EEPROM。

## 最新验证状态（2026-10-31）

- 用户反馈：除水泵、风扇、继电器相关部分外，其余 STM32 改动基本正常。
- 水泵、风扇、继电器：由于测试条件暂不方便而未测，先搁置；不应标记为故障，也不应写成全部实测通过。
- 主机回归：实际控制/PID/配置函数配合 GPIO、时钟和 EEPROM 模拟，32 项断言通过；修改前备份能重现风扇请求覆盖。
- Keil 完整重编译：0 错误、8 个现有警告（Key.c 3 个、cJSON 5 个）。
- App ArkTS 源码及资源编译通过，保留服务层 3 条现有异常处理警告。
- 新界面已预览；用户确认长截图中的重复、断层属于截图拼接，实际页面正常。
- 用户说明：截图采集时STM32未上电；截图仅用于界面展示，不据此新增联网故障记录。

## 最近维护入口

- 配置：modules/middleware/src/mw_config.c；滤波：modules/middleware/src/mw_filter.c。
- 模拟回归：tests/run_control_regressions.py，使用方法见tests/README.md。
- 完整修复与硬件待测项目见仓库docs/06。

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
