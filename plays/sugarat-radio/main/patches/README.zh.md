---
description: "保留的 LVGL 8.3.11 GIF 内存补丁与配置行为。"
---

# LVGL GIF 内存补丁

[English](README.md) | 中文

这些导入的 MIT 补丁面向 LVGL 8.3.11。Sugarat 电台使用索引色 C 图像，不显示上游开机 GIF。保留的解码补丁可供同一 BSP 构建的其他 GIF 使用方使用。

`apply_lvgl_patches.py` 在 CMake 配置时执行，将 `gifdec.c`、`gifdec.h` 和 `lv_gif.c` 成组复制到下载的 LVGL 组件。脚本逐字节比较，复制差异并校验写入内容。目标缺失时警告并跳过；复制或校验失败时终止配置。警告不代表补丁已经应用。

补丁在 PSRAM 分配 GIF 缓冲，使用 RGB565 画布，将内存 GIF 输入复制到独立 RAM，并在关闭时释放。解码声明、数据所有权与 LVGL 图像字段一起变化，三个文件必须兼容。保留其来源与许可声明。升级 LVGL 需要另行比较、构建并在真机验收 GIF，本玩法固定 8.3.11。
