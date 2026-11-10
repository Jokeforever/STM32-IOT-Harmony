# App 趋势逻辑回归

app-trend-regressions.cjs 读取实际 entry/src/main/ets/model/TrendHistory.ets，通过 DevEco SDK 自带的 TypeScript 编译器转译，在 Node 中执行断言。不联网、不读取密钥、不需要开发板，也不打 HAP。

当前结果：68 个场景、127 项检查通过。覆盖去重、乱序、同值不同时间、属性独立时间、数值与有效位、20 点上限、旧缓存迁移、两页面历史合并、时间轴及快照时间展示。

在 PowerShell 中运行：

```powershell
& 'F:\DEVECO\DevEco Studio\tools\node\node.exe' `
  'E:\harmony\HarmonyEmbeddedLab\tests\app-trend-regressions.cjs'
```

默认编译器位置为 F:/DEVECO/DevEco Studio/sdk/default/openharmony/ets/build-tools/ets-loader/node_modules/typescript/lib/typescript.js。其他电脑可指定 --compiler，或通过 --source 指定待检查的 TrendHistory.ets。测试时区固定 Asia/Shanghai，用于时间标签的确定性断言；生产 App 仍按运行设备本地时区显示。

这组测试只验证纯模型逻辑，不执行 ArkUI 的 @State/@Prop、导航、PersistentStorage 或 OneNET 网络。仍需 DevEco 源码编译，以及 docs/12-App-Report-Trends.md 中的五项联机和界面实测。

固件主机回归独立维护在 stm32-firmware/tests；本轮没有修改或重新验证固件功能。

新增缩放回归覆盖displayRange局部量程、恒定值与湿度边界、负温、样本不变，以及plotPoints的可配置高度。UI弹层、返回与大图持续刷新仍需实际操作验证。

连续视窗新增15场景43项，覆盖滚轮倍率、1～10倍限制、锚点、独立轴、平移、裁剪坐标、滑条映射和毫秒时间。原有趋势去重/缓存回归继续运行；不模拟真实轴事件或触屏路由，后者按docs/12实际操作验证。

用户将上限下调为10倍，生产边界和滑条共用MAX_ZOOM，原上限用例同步改为10倍预期。

## App控制回归

新增app-control-regressions.cjs，40场景152项检查。读取生产ControlTracker、OneNetApiService与提取的DeviceDetail方法，模拟网络、时钟、定时器和AppStorage。测试不访问OneNET，不加载真实配置，不执行ArkUI渲染。

```powershell
& 'F:\DEVECO\DevEco Studio\tools\node\node.exe' `
  'E:\harmony\HarmonyEmbeddedLab\tests\app-control-regressions.cjs'
```

可用--root指定App仓库根目录，--compiler指定SDK转译器。覆盖设备应答、上报匹配、期限、重复发送保护、旧回包、页面离开、关闭自动刷新与脉冲命令。联机和界面五项验收见docs/13。

## 2026-11-10 温度上下限

生产App支持tempthrehold通风上限和temp_low_threshold加热下限，沿用控制确认。控制回归现44场景168项，新增下限实际API参数、标签单位、配对冲突/缺项拒绝及下限新上报确认；趋势68场景127项保留。ArkTS编译通过3条既有服务警告，未生成HAP。用户已反馈原第1、2项及本机四项通过；风扇/加热器相关实测仍暂缓，第二参数页样式获免追加实测提交授权，见docs/21。
