# STM32 主机回归测试

这些测试在 Windows 主机上编译并执行实际生产 C 函数，使用驱动、GPIO、时钟、EEPROM 模拟，
不连接真实板子，不访问网络，不读取设备密钥。默认使用 DevEco SDK 自带 Clang；可通过 `--clang` 指定。

## 运行

在真实工程根目录执行：

```powershell
python tests/run_config_port_regressions.py
python tests/run_pid_regressions.py
python tests/run_sensor_model_regressions.py
python tests/run_control_regressions.py
```

四者都支持 `--firmware-root` 指定工程根目录、`--overlay` 指定候选修改目录；默认测试当前工程。
临时测试产物由临时目录管理，执行结束后自动清理。

- 配置接口测试：1000次新旧读写/时钟轨迹对比，覆盖延时、重试和回绕；真实适配的范围和转发检查包含在控制回归内。
- PID独立测试：实际mw_pid与冻结的拆分前函数比较，4096步差分和独立边界向量，36925项断言。
- 采集测试：实际 app_sensors/mw_filter 与冻结的重构前采集逻辑比较，1000组输入、9030项断言。
- 控制测试：实际 mw_pid/LED/jdq/beep/bump/Key/app_config/mw_config，313项断言，覆盖通风仲裁、PID、14类参数边界、按键、命令业务入口、旧EEPROM二进制兼容与失败恢复、模式保存和中断完成。
- 新配置测试执行实际app_config/LED/jdq/beep/bump/Key/mw_config代码。云端入口调用生产AppCommands；真实JSON与分发函数由新增命令测试覆盖，未覆盖真实MQTT网络。
- 这些不是HIL，也不意味着真实硬件全部通过或取得完整代码覆盖率。

最新LED端口编译及用户五项已通过的实测记录见 `docs/2026-11-10-LED-Port.md`。配置接口历史验收见 `docs/2026-11-07-Config-Port.md`。
PID分离历史验收见 `docs/2026-11-07-PID-Separation.md`。
传感器模型历史记录见 `docs/2026-11-01-Sensor-Model.md`。

运行配置重构、完整编译和本轮硬件验收见 `docs/2026-11-01-Runtime-Config.md`（2026-11-07用户五项验收通过）。

PID对照函数位于tests/fixtures/pid_before_extract.c，仅用于主机测试，不加入目标工程。

配置接口对照函数位于tests/fixtures/config_before_port.c，仅主机测试使用，不加入目标固件。

## 2026-11-09 命令处理回归

```powershell
python tests/run_command_regressions.py
```

1128项生产命令、业务适配、真实cJSON/OneJSON、提取MQTT分发及真实发送函数/主循环上报联动检查。编译器仍使用DevEco SDK Clang；新测试通过--windows-sdk定位本机Windows SDK的ucrt与kernel32导入库（默认自动选取已安装版本），不安装新依赖。
支持--firmware-root/--overlay/--clang。网络、GPIO输出边界与时钟模拟；真实JSON解析与分配失败清理不是手工伪JSON树。不是HIL，不连接OneNET，不使用或输出真实密钥。
原控制313项改为调用生产AppCommands入口；云端土壤冲突改为拒绝并保留原值，相应预期已更新。本机AppConfig和按键钳制规则未改。
旧cJSON畸形数字先由新增用例复现，再修正语法/大数转换；完整结果与五项烧录清单见docs/2026-11-09-Command-Service.md。用户五项验收通过后已同步仓库脱敏副本；用户已授权本轮提交上传。

## 2026-11-10 LED端口回归

```powershell
python tests/run_led_port_regressions.py
```

123616项断言，含4096步冻结旧实现/生产新实现对照和多周期千分比换算。公共LED.h与端口头单独在无HAL/按键/OLED头的环境编译。支持--firmware-root/--overlay/--clang/--windows-sdk，Windows SDK导入库只用于主机测试输出。
fixtures/led_before_port.c与.h仅用于测试，未加入目标工程。旧控制313项改为链接真实app_led_port，实现保留当前ARR=999的边界模拟；新回归另验证不同ARR与不支持周期的拒绝。不是HIL，不接网络或实物灯具。
真实工程编译0错误5条既有警告；用户于2026-11-10确认本轮五项实物检查均通过。确认后同步脱敏副本并按约定提交上传；实体负载测试继续暂缓。

## 2026-11-10 传感器读取接口回归（用户实物验收通过）

```powershell
python tests/run_sensor_port_regressions.py
python tests/run_sensor_model_regressions.py
```

新端口4625项：实际app_sensor_port.c配合驱动边界模拟，覆盖所有uint8返回值、DHT成功方向、整数边界、失败输出保留、空指针无底层调用等。实际服务/公开头在无HAL或具体驱动头环境编译。支持--firmware-root/--overlay/--clang。
原采集9030项现链接实际新端口，保留独立旧采集参考与1000组输入，继续验证调用顺序、有效位、滤波及失效恢复。另五套控制/命令/配置/PID/LED回归全部通过。
真实Keil完整重编译0错误5条既有cJSON警告；用户确认整板断电重启后联网及其他功能正常；验收后已同步仓库脱敏副本，按约定提交上传。学习与清单见docs/2026-11-10-Sensor-Port.md。不是HIL，不连接OneNET或使用真实密钥。

## 2026-11-10 按键输入接口回归（用户五项实测通过）

```powershell
python tests/run_key_port_regressions.py
python tests/run_control_regressions.py
```

新测试从生产Key.c提取Key_Scan原文，与冻结旧扫描函数及生产app_key_port.c共同编译。Key结构来自实际Key.h，GPIO边界模拟。6117步新旧状态/读取顺序对照、214535项断言通过；另用独立预期检查16种映射、短按/长按/释放边界、抖动/延迟消费、计数回绕与无HAL端口公开头。
支持--firmware-root/--overlay/--clang/--windows-sdk。Windows SDK导入库只用于主机输出，不参与固件。
原控制313项仍编译完整生产Key.c并链接新端口，补充Key_Func/配置/ISR集成检查。其他六套采集、LED、命令、配置、PID回归通过。
真实Keil完整重编译0错误5条既有cJSON警告；用户于2026-11-10明确确认本轮五项实物检查均通过；已同步仓库脱敏副本，按约定提交上传。学习与清单见docs/2026-11-10-Key-Port.md。不是HIL，不连接OneNET或实体负载。
