# SDGOODS 圆形徽章硬件资料

此页描述 `sdgoods-esp32s3-round`、360×360 圆屏版本的文档参数。它们来自现有 Sugarat 产品记录和所附基础模板，不是当前连接设备的检测结果；不适用于其他版本或外观相似的板卡。

## 原始资料

- [产品参数和来源](../boards/sdgoods-esp32s3-round.json)：机器可读的事实、来源和开发版本。
- [board_pins.h](../boards/board_pins.h)：模板内原始引脚定义的独立副本。
- [模板分区表](../boards/template-partitions.csv) 和 [默认配置](../boards/template-sdkconfig.defaults)：构建参考，不代表当前设备布局。
- [厂商源码](https://github.com/SDGOODS/SDGOODS-ESP32S3) 与 [厂商引脚文件](https://github.com/SDGOODS/SDGOODS-ESP32S3/blob/main/components/sdgoods_board/include/board_pins.h)：外部资料可能更新，本包以锁定资源为准。此包不包含独立 PDF 原理图或经确认的 PCB 修订号。

## 参数速查

| 项目 | 文档参数 |
| --- | --- |
| 目标和内存 | ESP32-S3；32 MiB Flash；8 MiB Octal PSRAM |
| SDK | ESP-IDF 5.5.4；LVGL 8.3.11；不可替换为 LVGL 9 适配器 |
| 显示 | ST77916，360×360，QSPI RGB565；主要内容安全区 x=60、y=60、w=240、h=240 |
| Flash / PSRAM | Flash DIO 80 MHz；Octal PSRAM 80 MHz |
| 显示接线 | SCK 40；D0..D3 46/45/42/41；CS 21；TE 18；RESET 9；电源 12；背光 13 |
| 触摸和共享 I²C | CST816，地址 0x15；SCL 10、SDA 11；INT 4、RESET 5；I²C port 1、400 kHz |
| IMU | QMI8658，共享 I²C，总线地址 0x6B |
| 电源和按键 | GPIO6，低电平有效；GPIO7 为高电平保持供电的锁存输出，不能当普通按钮 |
| 麦克风 | BCLK 16、WS 2、DIN 17 |
| 扬声器 | BCLK 48、WS 38、DOUT 47、MCLK 15；PA 3，高电平有效 |
| 电池采样 | ADC1 channel 7，不能将通道编号解释成 GPIO7 |

## 使用限制

优先通过 BSP 访问外设，保留电源锁存、触摸坐标转换和共享总线所有权。硬件资料缺失或设备与参考不符时先补充证据，不扫描未知引脚，也不通过试刷固件猜测控制器。

模板分区表的 factory 应用从 `0x10000` 开始，数据区从 `0x1000000` 开始；这些值只描述该模板。设备可能运行其他布局或使用不同启动槽。部署前按 [部署说明](deployment.md) 检查，不能据此直接写设备。
