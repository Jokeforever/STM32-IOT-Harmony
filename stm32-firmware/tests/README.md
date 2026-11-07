# STM32 主机回归测试

这些测试在 Windows 主机上编译并执行实际生产 C 函数，使用驱动、GPIO、时钟、EEPROM 模拟，
不连接真实板子，不访问网络，不读取设备密钥。默认使用 DevEco SDK 自带 Clang；可通过 `--clang` 指定。

## 运行

在真实工程根目录执行：

```powershell
python tests/run_pid_regressions.py
python tests/run_sensor_model_regressions.py
python tests/run_control_regressions.py
```

三者都支持 `--firmware-root` 指定工程根目录、`--overlay` 指定候选修改目录；默认测试当前工程。
临时测试产物由临时目录管理，执行结束后自动清理。

- PID独立测试：实际mw_pid与冻结的拆分前函数比较，4096步差分和独立边界向量，36925项断言。
- 采集测试：实际 app_sensors/mw_filter 与冻结的重构前采集逻辑比较，1000组输入、9030项断言。
- 控制测试：实际 mw_pid/LED/jdq/beep/bump/Key/app_config/mw_config，294项断言，覆盖通风仲裁、PID、14类参数边界、按键、云端属性分发片段、旧EEPROM二进制兼容与失败恢复、模式保存和中断完成。
- 新配置测试执行实际app_config/LED/jdq/beep/bump/Key/mw_config代码。云端分发使用模拟JSON对象，未覆盖真实MQTT网络。
- 这些不是HIL，也不意味着真实硬件全部通过或取得完整代码覆盖率。

最新目标工程编译与本轮实测清单见 `docs/2026-11-07-PID-Separation.md`，本轮五项已由用户确认通过。
传感器模型历史记录见 `docs/2026-11-01-Sensor-Model.md`。

运行配置重构、完整编译和本轮硬件验收见 `docs/2026-11-01-Runtime-Config.md`（2026-11-07用户五项验收通过）。

PID对照函数位于tests/fixtures/pid_before_extract.c，仅用于主机测试，不加入目标工程。
