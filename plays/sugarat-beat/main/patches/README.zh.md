---
description: "固件配置阶段应用的固定版本 LVGL GIF 内存补丁。"
---

# LVGL GIF 内存补丁

[English](README.md) | 中文

导入的 MIT 补丁面向 LVGL 8.3.11。Sugarat Beat 使用 LVGL 绘制游戏对象。

`apply_lvgl_patches.py` 在 CMake 配置时执行，将 `gifdec.c`、`gifdec.h` 和 `lv_gif.c` 成组复制到随附的本地 LVGL 组件，并逐字节校验。读取、复制或校验失败时停止配置。分发时保留[上游许可](../../components/lvgl/LICENCE.txt)和文件版权声明。
