# 谷仓电池监测

[English](README.md) | 中文

安装时选择 `sdgoods-battery`、`target=esp-idf`，目录使用谷仓固件工程根目录，并在 `main/REQUIRES` 中添加 `sugarat_battery`。在 `sdgoods_hw_info_init()` 之后调用 `sugarat_battery_start(&config)`，并检查返回的错误码。ADC1 由 BSP 管理，组件在独立任务中通过 `sdgoods_hw_bat_v()` 读取电压，不创建 ADC 实例。

```c
sugarat_battery_config_t battery = sugarat_battery_default_config();
battery.on_event = on_battery_event; /* runs on the battery task */
ESP_ERROR_CHECK(sugarat_battery_start(&battery));
```

## 读数与事件

组件每隔 `sample_period_ms`（默认 1000）读取五次取中位数，再对最近 `average_samples` 次（默认 10）求平均，用 `curve` 换算成百分比。`curve` 为 NULL 时使用内置单节锂电池静置曲线：4200 mV 对应 100 %，3300 mV 对应 0 %。百分比变化一个点后需连续保持五次采样才会发布。电量低于 `low_percent`（15）进入 LOW，低于 `critical_percent`（5）进入 CRITICAL；回升到阈值加 `hysteresis_percent`（3）以上才恢复。积累 30 秒数据后 `trend_mv_per_min` 才有效。开启 `monitor_usb` 时，`usb_connected` 表示连接了电脑等 USB 数据主机。`sugarat_battery_percent_from_mv()` 用同一曲线换算单个电压值。

`config.on_event` 接收 PERCENT、LEVEL 和 USB 变化事件，附带变化后的数值。回调在电池任务中执行：请复制事件或用 `sdgoods_lvgl_post()` 投递到 LVGL 线程，不要在回调中调用 LVGL。组件运行期间，自定义曲线必须保持有效。

## 精度

百分比是估算值：背光或扬声器负载会拉低电压，充电会抬高电压。电路板没有公开的充电状态引脚；只充电、不传数据的 USB 电源不会显示为已连接。需要准确电量时，请提供针对实际电芯测得的曲线。没有返回读数的读取计入 `failed_reads` 并被跳过；取得第一个有效读数之前，电量状态保持 UNAVAILABLE。已在一块谷仓圆屏徽章上完成实物验收，覆盖电压、百分比、电量状态、变化趋势和 USB 主机检测。
