# 执行器与 PID 回归测试

测试直接编译 LED.c、jdq.c、beep.c 和 mw_config.c，模拟 GPIO、HAL 时钟和 EEPROM；
不需要连接 STM32，不会联网或加载设备密钥。

Windows 下运行（需可生成 Windows 程序的 Clang 和 lld-link）：

```powershell
python .\run_control_regressions.py --clang 'F:\DEVECO\DevEco Studio\sdk\default\openharmony\native\llvm\bin\clang.exe'
```

测试共 32 项断言，覆盖温度/CO2 风扇仲裁、报警不覆盖输出、继电器状态释放、
无效测量、阈值边界、PID 的 0.5/1/1.5 秒间隔、毫秒计数回绕、长间隔恢复、
PWM 限幅和 EEPROM 参数保留。

它不替代 Keil 目标编译、App 编译或实体接线验证。传感器失效时撤销该传感器的
自动控制请求，是当前学习项目策略；不代表其他用途的通用故障安全策略。
