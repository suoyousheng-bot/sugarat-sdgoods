# SDGOODS battery monitor

English | [中文](README.zh.md)

Install `sdgoods-battery` with `target=esp-idf` under the SDGOODS firmware project root and add `sugarat_battery` to `main/REQUIRES`. Call `sugarat_battery_start(&config)` after `sdgoods_hw_info_init()` and check the returned error. The BSP owns ADC1; the component reads through `sdgoods_hw_bat_v()` on its own task and creates no ADC unit.

```c
sugarat_battery_config_t battery = sugarat_battery_default_config();
battery.on_event = on_battery_event; /* runs on the battery task */
ESP_ERROR_CHECK(sugarat_battery_start(&battery));
```

## Readings and events

Every `sample_period_ms` (default 1000) the component takes the median of five reads, averages the last `average_samples` (default 10) and converts the average with `curve`. A NULL curve selects the built-in single-cell Li-ion rest curve from 4200 mV at 100 % to 3300 mV at 0 %. A one-point percentage change is published after it persists for five samples. LOW and CRITICAL start at `low_percent` (15) and `critical_percent` (5), and recovery needs the threshold plus `hysteresis_percent` (3). `trend_mv_per_min` becomes valid after 30 s of history. With `monitor_usb`, `usb_connected` reports a USB data host such as a computer. `sugarat_battery_percent_from_mv()` converts a single voltage with the same curve.

`config.on_event` receives PERCENT, LEVEL and USB changes with the values after the change. It runs on the battery task: copy the event or post it with `sdgoods_lvgl_post()`, and never call LVGL there. A custom curve must stay valid while the component runs.

## Accuracy

The percentage is an estimate. Load from the backlight or speaker lowers the voltage and charging raises it. The board exposes no documented charge-status pin, and a charger without USB data is not reported as connected. Supply a curve measured for the installed cell when accuracy matters. Reads that return no value count as `failed_reads` and are skipped; the level stays UNAVAILABLE until the first valid reading. Hardware acceptance on an SDGOODS round badge covered voltage, percentage, level, trend and USB host detection.
