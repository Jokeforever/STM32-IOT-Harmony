# STM32-IOT-Harmony

STM32 + ESP8266 + OneNET + HarmonyOS 的单节点环境监测与控制项目。

```text
传感器 → STM32F103 → ESP8266 → OneNET 旧版物模型 → HarmonyOS App
执行器 ← STM32F103 ← 属性下发 ← OneNET ← App 控制与阈值设置
```

## 当前实现

- STM32 裸机主循环、中断接收、定时采集与约 5 秒属性上报。
- DHT11、JW01、模拟光照与土壤湿度采集，滤波及部分传感器有效性上报。
- LED 位置式 PID、脉冲/自动灌溉、温度/CO2 通风请求合并。
- 24C02 双槽、CRC32、配置 v1→v2 迁移及延时保存。
- ESP8266 TCP/MQTT 重连和看门狗。
- App 深绿/薄荷绿/暖白界面、设备读数、控制、阈值、趋势及缓存。
- App 10 秒自动刷新、30 秒属性时间戳在线判断；未读取数据时显示 `--`。

当前硬件仍为 **F103 + ESP8266**。F407/S3 新底板仅作未来方案讨论，未投入使用；没有引入 RTOS。
App 首页版本标记仍为 v13，STM32 配置格式仍为 v2；本轮未调整包版本。

## 最新验证状态（2026-10-31）

- 用户反馈：除水泵、风扇、继电器相关部分外，其余 STM32 改动基本正常。
- 水泵、风扇、继电器：由于测试条件暂不方便而未测，先搁置；不应标记为故障，也不应写成全部实测通过。
- 主机回归：实际控制/PID/配置函数配合 GPIO、时钟和 EEPROM 模拟，32 项断言通过；修改前备份能重现风扇请求覆盖。
- Keil 完整重编译：0 错误、8 个现有警告（Key.c 3 个、cJSON 5 个）。
- App ArkTS 源码及资源编译通过，保留服务层 3 条现有异常处理警告。
- 新界面已预览；用户确认长截图中的重复、断层属于截图拼接，实际页面正常。
- 用户说明：截图采集时STM32未上电；截图仅用于界面展示，不据此新增联网故障记录。

## 文档导航

- [鸿蒙 App 架构与学习说明](docs/01-HarmonyOS-App-Architecture.md)
- [调试记录与经验](docs/02-HarmonyOS-App-Debugging-Notes.md)
- [STM32 功能模块与学习说明](docs/03-STM32-Firmware-Modules.md)
- [当前进度与后续计划](docs/04-Project-Progress-Summary.md)
- [AI 接手说明](docs/05-AI-Handoff.md)
- [本轮控制修复与实测清单](docs/06-Control-Fixes-Validation.md)
- [App 界面美化记录](docs/07-App-Visual-Refresh.md)
- [更新日志](CHANGELOG.md)

## 修改、验证与同步约定

1. STM32 先修改并编译 `E:\fuchuang\code\IOT5-smart2-IOT-final`。
2. 等用户确认效果，再把功能变更同步到本仓库的 `stm32-firmware` 脱敏副本。
3. App 直接修改 `E:\harmony\HarmonyEmbeddedLab`。
4. 是否提交和上传 Git 由用户明确决定，验证正常不等于自动授权上传。
5. 真实 WiFi、OneNET 设备密钥和 App 用户密钥不得进入仓库。
6. 源码保持 UTF-8；未明确要求时不生成 HAP，不自动烧录、不清空 EEPROM。

## 构建与运行

### STM32

维护时使用真实配置工程：

```text
E:\fuchuang\code\IOT5-smart2-IOT-final\MDK-ARM\IOT.uvprojx
```

仓库副本的 WiFi/设备密钥为占位符，不能直接用于真实联网烧录。
第三方克隆后应按 [固件说明](stm32-firmware/README.md) 在本地填写自己的配置。

### HarmonyOS App

使用 DevEco Studio 6.0.2、SDK 6.0.2(22) 打开工程。
参考 `entry/src/main/ets/config/OneNetConfig.sample.ets` 创建本地 `OneNetConfig.ets`；真实配置文件已忽略。
App 使用 OneNET 用户级凭据，固件使用设备级凭据，两者不能混用。

### 主机回归测试

参见 [测试说明](stm32-firmware/tests/README.md)。测试直接编译相关生产 C 函数并模拟硬件，不需要连接开发板。

## 已知限制与下一步

- `water_vol` 是估算补水量，不是实际测得的出水量。
- `sensor_status` 仅使用 bit0 温度、bit1 湿度、bit3 CO2，正常通常为 11；光照/土壤/水量不判断拔插有效性。
- 灯带与光照传感器的光学耦合仍受实际布置限制，PID 未完成实际整定。
- 图表保留最近 20 次查询样本，时间戳去重尚未实现。
- 下一步候选是统一数据模型和进一步分离算法/驱动；**尚未开始实施**。
