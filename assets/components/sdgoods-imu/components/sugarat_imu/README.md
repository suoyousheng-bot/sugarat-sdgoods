# SDGOODS IMU

English | [中文](README.zh.md)

Install `sdgoods-imu` with `target=esp-idf` under the SDGOODS firmware project root and add `sugarat_imu` to `main/REQUIRES`. Call `sugarat_imu_start(&config)` after `sdgoods_touch_init()` and `sdgoods_hw_info_init()` and check the returned error. The component probes the QMI8658 at 0x6B on the I2C port installed by the touch driver, configures ±8 g and ±512 dps, and samples on its own task at 100 Hz. While it runs, it owns the sensor configuration: `sdgoods_hw_gyro()` scaling no longer applies, and every 2 s the component restores ranges changed by another owner and counts the repair in `config_repairs`.

```c
sugarat_imu_config_t imu = sugarat_imu_default_config();
imu.on_event = on_imu_event; /* runs on the IMU task */
ESP_ERROR_CHECK(sugarat_imu_start(&imu));
```

## Readings and events

`sugarat_imu_get_state()` copies filtered acceleration in g, bias-corrected angular rate in dps, pitch and roll from gravity, temperature, orientation, rest state, event counters, read errors and the measured sample rate. Values use the device frame: +X toward the screen's right edge, +Y toward its top edge and +Z out of the screen. The default `axis_map` `{2, -1, 3}` matches the SDGOODS round badge mounting; other boards supply their own map. Gyro bias is learned while the device rests, and `gyro_bias_learned` stays false until the first rest period.

`config.on_event` receives SHAKE, FLIP_DOWN, FLIP_UP, PICKUP, STILL, STEP and ORIENTATION with the running count for that kind. It runs on the IMU task: copy the event or post it with `sdgoods_lvgl_post()`, and never call LVGL there. An orientation change requires 400 ms of stable gravity. A shake requires `shake_peaks` peaks above `shake_threshold_g` within `shake_window_ms`, followed by `shake_cooldown_ms`. Pickup is reported when rotation exceeds `pickup_gyro_dps` within 1.5 s after a rest of `still_ms`. The pedometer reports steps after four consecutive steps, so its first event adds four; `step_detection=false` disables it. `sugarat_imu_reset_counters()` clears the counters.

## Shared I2C bus

Touch and the IMU share I2C port 1. Reads use a 90 ms timeout. After a read slower than 20 ms or a failed read, the component skips samples with a back-off that grows in 20 ms steps up to 300 ms, and it never reads in bursts to catch up. Flash writes stall the bus; this policy matches the touch driver and prevents repeated legacy I2C bus resets that starve the LVGL task. `read_errors` and a logged warning report each stall, and `sample_hz` drops during back-off.

## Verification limits

Hardware acceptance on an SDGOODS round badge with ESP-IDF 5.5.4 covered 100 Hz sampling, tilt direction, face-up, face-down and upright orientation, shake, flip, pickup and step counting. Thresholds suit a handheld or worn badge; adjust them for other uses. Step counts are estimates. The component installs no UI and does not change BSP pins, partitions or the application entry point.
