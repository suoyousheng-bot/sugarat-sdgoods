# 谷仓 IMU 运动传感器

[English](README.md) | 中文

安装时选择 `sdgoods-imu`、`target=esp-idf`，目录使用谷仓固件工程根目录，并在 `main/REQUIRES` 中添加 `sugarat_imu`。在 `sdgoods_touch_init()` 和 `sdgoods_hw_info_init()` 之后调用 `sugarat_imu_start(&config)`，并检查返回的错误码。组件通过触摸驱动安装的 I2C 端口探测 0x6B 地址上的 QMI8658，配置为 ±8 g 和 ±512 dps，并在独立任务中以 100 Hz 采样。组件运行期间独占传感器配置：`sdgoods_hw_gyro()` 的换算不再适用；组件每 2 秒检查一次量程，被其他代码改动时恢复配置并计入 `config_repairs`。

```c
sugarat_imu_config_t imu = sugarat_imu_default_config();
imu.on_event = on_imu_event; /* runs on the IMU task */
ESP_ERROR_CHECK(sugarat_imu_start(&imu));
```

## 读数与事件

`sugarat_imu_get_state()` 复制以下数据：滤波后的加速度（g）、扣除零偏的角速度（dps）、由重力算出的俯仰角和横滚角、温度、朝向、静止状态、事件计数、读取错误数和实测采样率。数值使用设备坐标系：+X 指向屏幕右边缘，+Y 指向上边缘，+Z 垂直屏幕向外。默认 `axis_map` 为 `{2, -1, 3}`，对应谷仓圆屏徽章的芯片安装方向；其他电路板需提供自己的映射。陀螺仪零偏在设备静止时学习，首次静止之前 `gyro_bias_learned` 保持 false。

`config.on_event` 接收 SHAKE、FLIP_DOWN、FLIP_UP、PICKUP、STILL、STEP 和 ORIENTATION 事件，并附带该类事件的累计次数。回调在 IMU 任务中执行：请复制事件或用 `sdgoods_lvgl_post()` 投递到 LVGL 线程，不要在回调中调用 LVGL。重力方向稳定 400 ms 后才会切换朝向。在 `shake_window_ms` 内出现 `shake_peaks` 次超过 `shake_threshold_g` 的峰值才算一次摇晃，之后进入 `shake_cooldown_ms` 冷却。设备静止 `still_ms` 后，若 1.5 秒内转动超过 `pickup_gyro_dps`，即报告拿起。计步器在连续检测到四步后才开始报告，因此第一次事件会一次加四步；设置 `step_detection=false` 可关闭计步。`sugarat_imu_reset_counters()` 将计数清零。

## 共享 I2C 总线

触摸和 IMU 共用 I2C 端口 1。读取超时为 90 ms。某次读取耗时超过 20 ms 或失败后，组件会暂停采样，暂停时长每次增加 20 ms，最长 300 ms；恢复后不会连续补读。写入 Flash 时总线会被阻塞，这套策略与触摸驱动一致，避免旧版 I2C 驱动反复复位总线，导致 LVGL 任务得不到运行时间。每次阻塞都会计入 `read_errors` 并输出警告日志，暂停期间 `sample_hz` 会下降。

## 验证范围

已在一块谷仓圆屏徽章上（ESP-IDF 5.5.4）完成实物验收，覆盖 100 Hz 采样、倾斜方向、屏幕朝上/朝下/竖立三种朝向、摇晃、翻转、拿起和计步。阈值按手持或佩戴的徽章调校，其他用途请自行调整。计步结果是估算值。组件不安装界面，也不修改 BSP 引脚、分区表或应用入口。
