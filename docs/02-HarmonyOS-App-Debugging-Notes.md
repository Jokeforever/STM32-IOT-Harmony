# 鸿蒙 App 调试经验总结（从连接失败到初步完成）

## 1. 今天调试的总目标

目标链路：

```text
STM32 传感器
  ↓
ESP8266 WiFi
  ↓
OneNET MQTT
  ↓
OneNET 物模型
  ↓
iot-api.heclouds.com 旧版 API
  ↓
HarmonyOS App
```

最终完成了：

- STM32 数据成功进入 OneNET 物模型
- 鸿蒙 App 成功读取数据
- 主页面显示温湿度、在线状态、最后同步
- 详情页实时刷新
- LED 模式控制
- 水泵模式控制
- 在线/离线状态判断

## 2. 第一阶段：App 连不上 OneNET

### 现象

- 鸿蒙 App 查询失败
- OneNET 返回“订购关系鉴权不通过”
- 一度怀疑需要购买或订购服务

### 原因

当时使用的是新版 DMP/AIoT 接口思路，和当前工程实际使用的旧版 OneNET 平台不匹配。

### 解决

改回旧版接口：

```text
https://iot-api.heclouds.com/thingmodel/query-device-property
https://iot-api.heclouds.com/thingmodel/set-device-property
```

鉴权方式：

```text
user_id + author_key
version = 2022-05-01
resource = userid/用户ID
```

经验：

- 先确认平台到底是新版还是旧版
- 看 STM32 端使用的 MQTT 地址和产品/设备资源
- 旧版 OneNET 不一定需要订购

## 3. 第二阶段：本地 Token 生成问题

### 现象 1：`newByteLength is out of range`

原因：

- ArkTS `buffer` 与 `Uint8Array` 的构造方式出现兼容问题
- 用 `.buffer + byteOffset + length` 构造时越界

解决：

- 使用 `util.Base64Helper` 解码 author_key
- 使用 `util.TextEncoder` 生成 UTF-8 字节

### 现象 2：OneNET 返回 10403 invalid authorization

验证过程：

1. 用标准 Node.js HMAC-SHA1 计算同一组参数
2. 对比 App 输出的签名前 8 位
3. 结果一致，说明算法正确
4. 同一把 key 查询旧项目产品和当前产品都失败

结论：

- key 本身已失效/被重新生成
- 更新 OneNET 用户访问密钥后，鉴权恢复

经验：

- 签名算法正确不代表 key 有效
- 可以用“同一把 key 查旧产品和当前产品”做对照
- 不要把 key、Token 明文输出到公开文档

## 4. 第三阶段：STM32/ESP8266 连接问题

### 现象

- OLED 卡在 `4. CWJAP...`
- 手机热点没有收到连接

### 子问题 1：UART 接收首字节丢失

原因：

- `main.c` 一开始让 USART2 接收到 `esp8266_buf`
- 回调里却把 `received_data` 写进 `esp8266_buf`
- 第一个字节被覆盖丢失

解决：

```c
HAL_UART_Receive_IT(&huart2, &received_data, 1);
```

并修正：

```c
extern unsigned short esp8266_cnt;
```

### 子问题 2：`UsartPrintf` 死循环

原因：

- 发送循环里 `pStr` 永远不向后移动
- 一直发送第一个字符

解决：

```c
len = strlen((char *)UsartPrintfBuf);
HAL_UART_Transmit(huart, UsartPrintfBuf, len, HAL_MAX_DELAY);
```

### 子问题 3：CWJAP 超时太短

原代码只等 2 秒，而 WiFi 连接可能超过 2 秒。

调试时曾延长到 30 秒，后来根据用户要求恢复原版。

### 子问题 4：手机热点是 5GHz

这是最终根因：

- ESP8266 只支持 2.4GHz
- 5GHz 热点对 ESP8266 等于不存在
- `AT/CWMODE/CWDHCP` 能过，因为它们是本地 AT 命令
- 一到 `CWJAP` 就失败或没有响应

解决：

- 手机热点切到 2.4GHz
- 加密方式用 WPA2
- 保证 ESP8266 供电足够

经验：

- ESP8266 调试先查 2.4GHz、供电、AT 原始回复
- 不要一上来就怀疑云端或 App

## 5. 第四阶段：OneNET 物模型没有数据

### 现象

- OneNET 网页能看到数据
- App 详情页属性全是 `--`
- `query-device-property` 只返回属性模型，不返回 value

### 原因

STM32 属性上报报文缺少：

```json
"version": "1.0"
```

### 解决

在 `onenet.c` 的属性上报 JSON 中增加：

```c
strcpy(buf, "{\"id\":\"123\",\"version\":\"1.0\",\"params\":{");
```

经验：

- OneNET 物模型属性上报必须带 `id`、`version`、`params`
- 网页能看到原始数据，不代表物模型当前属性有值

## 6. 第五阶段：App 能连上但界面不刷新

这是今天最耗时的一类问题，核心都是 ArkUI 响应式更新机制。

### 6.1 HTTP GET 缓存

现象：

- 主页面第一次拿到空数据后，一直显示 0
- 详情页偶尔能看到数据

原因：

- HarmonyOS `http.request` 的 `usingCache` 默认是 `true`

解决：

```ets
usingCache: false
```

并给 URL 增加时间戳：

```text
&_t=当前时间戳
```

### 6.2 带参数的 `@Builder` 不刷新

现象：

- 主页面状态调试显示 `温度=25 湿度=39 数据项=12`
- 但卡片还是显示 `0℃ / 0% / 0项`

原因：

- `@Builder metric(label, value, unit)` 不随参数变化重新渲染

解决：

- 把带参数的 Builder 改成带 `@Prop` 的组件
- 或者直接在父组件里内联渲染

涉及位置：

- 顶部 `StatChip`
- 主页面 `DeviceCard`
- 详情页 `DetailHeaderInfo`
- 详情页 `DetailMetricCard`
- 详情页 `DetailPropertyRow`
- 详情页 `DetailModeButton`

### 6.3 `ForEach` key 不变导致“全部属性”不刷新

原因：

- key 只用了 `identifier`
- 属性值变化时 key 不变，ArkUI 不重建行

解决：

```text
identifier + value + time
```

### 6.4 在线状态一直显示在线

原因：

- STM32 断电后，OneNET 仍然保留最后一次属性值
- 用“有没有值”判断在线是不准确的

解决：

- 读取属性里的 `time`
- 最后一次上报时间在 30 秒内才算在线
- 超过 30 秒显示离线

### 6.5 详情页刷新需要退出重进

原因：

- 详情页刷新被 `isRealDevice` 判断挡住
- 导航参数丢失时，`refresh()` 和自动刷新被跳过

解决：

- 当前工程只有一个实机，详情页固定按实机处理
- 移除 `isRealDevice` 对刷新逻辑的限制

### 6.6 调试计数器方法

在详情页加入：

```text
调试：点击=X 执行=Y loading=false
```

结果：

- 点击计数和执行计数都增加
- 证明请求执行了
- 最后定位到是显示层没有刷新

经验：

- 当“功能好像没生效”时，先区分是点击没触发、请求没执行，还是 UI 没刷新
- 可见计数器和时间戳比主观判断有效

## 7. 第六阶段：水泵控制不生效

### 现象

- OneNET 日志里 `{"led":1}` 成功
- `{"bump":0}` 有时成功、有时失败
- 水泵模式切换无效

### 排查

确认：

- App 的 `bump` 下发格式和 `led` 一致
- OneNET 已经收到 `{"bump":0}`
- 所以 App 请求格式没有问题

检查 STM32：

```c
if(bump_value == 0) { }
else if(bump_value == 1) { }
else if(bump_value == 2) { }
```

原因：

- STM32 收到 `bump` 后没有任何动作

解决：

```c
Bump_Set(0);
Bump_Set(1);
Bump_Set(2);
```

并在 `bump.h` 中导出：

```c
void Bump_Set(int mode);
```

经验：

- OneNET 日志成功只说明平台收到了写请求
- 还要确认 STM32 是否处理该属性
- 控制类问题要按“App → 云 → 设备 → 执行器”逐层检查

## 8. 今天形成的调试方法论

### 8.1 分层隔离

不要同时怀疑所有环节，按层排查：

1. 物理层：供电、接线、2.4GHz、串口
2. 设备层：STM32、ESP8266、AT 指令、MQTT
3. 云平台层：OneNET 物模型、属性上报、操作日志
4. App 层：鉴权、请求、数据解析、状态刷新
5. UI 层：`@State`、`@Prop`、`@Builder`、`ForEach` key

### 8.2 用证据而不是感觉

- OneNET 操作记录
- USART1 原始 AT 回复
- 模拟器截图
- `uitest dumpLayout`
- 页面可见调试计数
- 标准 Node.js 计算结果

### 8.3 先查物理层

ESP8266 最典型的问题：

- 热点是 5GHz
- 供电电流不足
- WPA3 不支持
- TX/RX 接反

### 8.4 ArkUI 状态刷新规则

- 顶层 `@State` 最适合保存页面数据
- `@Prop` 组件适合接收会变化的参数
- 带参数的 `@Builder` 慎用于动态数据
- `ForEach` key 要包含会变化的值

## 9. 最终结果

今天最终完成：

- OneNET 旧版鉴权
- STM32 物模型属性上报
- App 数据读取与实时显示
- 在线/离线判断
- 主页面仪表盘
- 详情页分组显示
- LED 模式控制
- 水泵模式控制
- 全部属性实时刷新

## 10. 后续注意事项

- ESP8266 继续使用 2.4GHz + WPA2
- App 里的 OneNET 用户密钥不要公开
- 正式产品建议把 Token 生成移到服务器
- STM32 重新烧录后要确认 `bump` 控制实际生效
- 后续新增动态 UI 时继续遵守 ArkUI 状态刷新规则
