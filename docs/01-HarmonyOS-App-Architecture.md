# 鸿蒙端 App 代码功能分区讲解（完整版）

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

> 截至 2026-10-07，STM32 端新增了 24C02 EEPROM 参数掉电保存，但 App 使用的 OneNET 属性标识符没有变化。App 不需要修改就能继续读取和下发已有的 `led`、`bump` 等属性。
> 传感器读取异常时，STM32 当前会保留最后一次有效值；`sensor_status` 还没有作为 OneNET 属性上报，因此 App 暂时不能显示“无效”状态，只能看到数值冻结。
> 2026-10-10 增加的传感器滤波只在 STM32 内部处理，不改变 OneNET 属性格式和 App 接口。

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
