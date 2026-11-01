# STM32 主机回归测试

这些测试在 Windows 主机上编译并执行实际生产 C 函数，使用驱动、GPIO、时钟、EEPROM 模拟，
不连接真实板子，不访问网络，不读取设备密钥。默认使用 DevEco SDK 自带 Clang；可通过 `--clang` 指定。

## 运行

在真实工程根目录执行：

```powershell
python tests/run_sensor_model_regressions.py
python tests/run_control_regressions.py
```

两者都支持 `--firmware-root` 指定工程根目录、`--overlay` 指定候选修改目录；默认测试当前工程。
临时测试产物由临时目录管理，执行结束后自动清理。

- 采集测试：实际 app_sensors/mw_filter 与冻结的重构前采集逻辑比较，1000组输入、9030项断言。
- 控制测试：实际 LED/jdq/beep/Key/mw_config，40项断言，覆盖通风仲裁、失效条件、PID时间、配置和温度阈值下边界。
- 这些不是HIL，也不意味着真实硬件全部通过或取得完整代码覆盖率。

最新目标工程编译与实测清单见 `docs/2026-11-01-Sensor-Model.md`。
