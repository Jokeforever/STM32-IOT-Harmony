# 鸿蒙端 App 代码功能分区讲解

## 1. 项目定位

这是一个基于 HarmonyOS ArkTS 的物联网监控 App，主要完成：

- 连接 OneNET 旧版物模型 API
- 读取 STM32 上报的传感器数据
- 显示在线状态、最后同步时间、温湿度、光照、CO2、土壤湿度、水量等
- 下发 LED 模式控制
- 下发水泵模式控制
- 详情页实时刷新和自动刷新

开发环境：

- DevEco Studio 6.0.2 Release
- Compatible SDK：6.0.2(22)
- 语言：ArkTS
- 应用包名：`com.example.harmonyembeddedlab`

## 2. 代码目录结构

核心目录：

```text
entry/src/main/ets/
├── config/
│   └── OneNetConfig.ets
├── model/
│   └── DeviceModels.ets
├── service/
│   └── OneNetApiService.ets
├── pages/
│   ├── Index.ets
│   └── DeviceDetail.ets
├── entryability/
└── entrybackupability/
```

资源与权限：

```text
entry/src/main/module.json5
entry/src/main/resources/base/element/string.json
entry/src/main/resources/base/element/color.json
```

## 3. 分层设计

### 3.1 配置层 `OneNetConfig.ets`

作用：保存 OneNET 旧版用户级鉴权参数。

当前包含：

```ets
USER_ID
ACCESS_KEY
PRODUCT_ID
DEVICE_NAME
```

说明：

- `USER_ID`：OneNET 用户 ID
- `ACCESS_KEY`：用户级 author_key，不是设备 MQTT 密钥
- `PRODUCT_ID`：当前产品 ID，例如 
- `DEVICE_NAME`：当前设备名，例如 

安全提醒：

- 当前是学习/比赛项目，密钥直接放在 App 内
- 正式产品应把 Token 生成放到服务器，App 不直接持有 author_key

### 3.2 数据模型层 `DeviceModels.ets`

主要类型：

| 类型 | 作用 |
|---|---|
| `DeviceState` | 在线/离线枚举 |
| `LedMode` | LED 模式枚举 |
| `DeviceInfo` | 设备基础信息 |
| `DeviceProperty` | OneNET 属性项 |
| `DeviceCloudStatus` | 云端设备状态 |
| `DeviceDetailParam` | 页面跳转参数 |
| `PropertyHelper` | 属性读取、中文名、单位、模式转换 |

`DeviceProperty` 保存：

```ets
identifier
value
name?
dataType?
accessMode?
time?
```

`PropertyHelper` 主要负责：

- 根据 `identifier` 找属性值
- 把 `temp`、`humi` 等英文标识转成中文名
- 补单位，例如 `℃`、`%`、`lx`、`ppm`
- 把 `led`、`bump` 的数字转成“关闭/常亮/自动”等文本

### 3.3 服务层 `OneNetApiService.ets`

作用：统一封装 OneNET 旧版 API 请求。

调用地址：

```text
https://iot-api.heclouds.com
```

主要方法：

| 方法 | 作用 |
|---|---|
| `queryDeviceProperties()` | 查询设备物模型属性 |
| `queryDeviceStatus()` | 查询设备在线状态和最后在线时间 |
| `setLedMode(mode)` | 设置 LED 模式 |
| `setBumpMode(mode)` | 设置水泵模式 |
| `setDeviceProperty(identifier, value)` | 通用属性下发 |
| `createAuthorization()` | 生成旧版用户级 Authorization |

鉴权方式：

- 版本：`2022-05-01`
- 资源：`userid/{USER_ID}`
- 方法：HMAC-SHA1
- 待签名字符串：

```text
et + "\n" + "sha1" + "\n" + "userid/用户ID" + "\n" + "2022-05-01"
```

实现要点：

- 使用 `util.Base64Helper` 解码 author_key
- 使用 `util.TextEncoder` 生成 UTF-8 字节
- 使用纯 ArkTS SHA1/HMAC-SHA1 实现，避免 cryptoFramework 版本差异
- GET 请求设置 `usingCache: false`
- URL 增加 `_t=时间戳`，避免 OneNET 返回缓存结果

### 3.4 首页 `Index.ets`

首页是监控仪表盘，主要状态：

| 状态 | 作用 |
|---|---|
| `deviceName` | 设备名称 |
| `deviceOnline` | 在线状态 |
| `temperature` | 温度 |
| `humidity` | 湿度 |
| `properties` | 全部属性 |
| `lastSyncTime` | App 最后同步时间 |
| `lastOnlineTime` | 设备最后上报时间 |
| `autoRefresh` | 自动刷新开关 |
| `loading` | 请求中状态 |

主要逻辑：

- `aboutToAppear()`：进入首页时同步一次并启动定时刷新
- `refreshOneNetData()`：查询属性并更新首页状态
- `syncOneNet()`：带错误处理的同步入口
- `startAutoRefresh()`：每 10 秒自动刷新
- `openDetail()`：进入详情页，并暂停首页定时器
- `buildDetailResult()`：构造详情页参数

首页 UI 组件：

| 组件 | 作用 |
|---|---|
| `StatChip` | 顶部在线设备、版本号、最后同步 |
| `DeviceCard` | 环境节点卡片 |

首页顶部：

```text
在线设备 / 版本号 / 最后同步
```

首页不再放 LED 控制块，控制全部移到详情页。

### 3.5 详情页 `DeviceDetail.ets`

详情页负责：

- 显示设备头部信息
- 显示环境数据
- 显示并控制 LED 模式
- 显示并控制水泵模式
- 显示阈值
- 显示全部属性
- 支持手动刷新和 10 秒自动刷新

主要状态：

| 状态 | 作用 |
|---|---|
| `deviceId` | 设备 ID |
| `deviceName` | 设备名称 |
| `online` | 在线状态 |
| `temperature` | 温度 |
| `humidity` | 湿度 |
| `properties` | 全部属性 |
| `lastSyncTime` | 最后同步时间 |
| `lastOnlineTime` | 最后在线时间 |
| `lastReportTime` | 最后上报时间戳 |
| `ledMode` | LED 模式 |
| `bumpMode` | 水泵模式 |
| `autoRefresh` | 自动刷新开关 |

详情页组件：

| 组件 | 作用 |
|---|---|
| `DetailHeaderInfo` | 设备 ID、最后同步、最后在线 |
| `DetailMetricCard` | 环境数据卡片 |
| `DetailPropertyRow` | 普通属性行 |
| `DetailModeButton` | LED/水泵模式按钮 |

在线判断：

```text
最后一次属性上报时间在 30 秒内 → 在线
超过 30 秒 → 离线
```

这样可以解决“STM32 断电后，OneNET 还保留旧属性值，App 一直显示在线”的问题。

## 4. 数据流

### 4.1 数据读取

```text
STM32
  ↓ MQTT 属性上报
OneNET 物模型
  ↓ iot-api.heclouds.com
OneNetApiService.queryDeviceProperties()
  ↓
Index / DeviceDetail 顶层 @State
  ↓
ArkUI 页面刷新
```

### 4.2 控制下发

```text
详情页模式按钮
  ↓
DeviceDetail.setLedMode() / setBumpMode()
  ↓
OneNetApiService.setDeviceProperty()
  ↓
OneNET thingmodel/set-device-property
  ↓
STM32 MQTT set 主题
  ↓
LED_Set() / Bump_Set()
  ↓
STM32 下一次属性上报
  ↓
App 刷新模式状态
```

## 5. ArkUI 状态刷新经验

本项目中很重要的一类问题是：数据请求成功，但界面不刷新。

结论：

1. 页面数据尽量用顶层 `@State` 字段保存。
2. 不要把会变化的 UI 放在带参数的 `@Builder` 中。
3. 带参数的 `@Builder` 可能不随参数变化重新渲染。
4. 需要响应式更新时，使用带 `@Prop` 的独立组件，或者直接内联渲染。
5. `ForEach` 的 key 如果只写 `identifier`，值变化时可能不会重建行。
6. 需要实时更新的 `ForEach`，key 应包含值和时间，例如：

```text
identifier + value + time
```

## 6. 当前 App 功能清单

- [x] OneNET 旧版用户级鉴权
- [x] 查询物模型属性
- [x] 查询设备在线状态
- [x] 显示温湿度、光照、CO2、土壤湿度、水量
- [x] 在线/离线状态
- [x] 最后同步时间
- [x] 自动刷新开关
- [x] 详情页手动刷新
- [x] 详情页自动刷新
- [x] LED 模式控制
- [x] 水泵模式控制
- [x] 全部属性实时展示
- [x] 界面美化和分组

## 7. 后续可扩展方向

- 温湿度历史曲线
- 多设备管理
- 阈值参数从 App 下发
- 下拉刷新
- 深色模式
- 网络异常重试
- 后端代理 OneNET 密钥
- 设备离线告警
