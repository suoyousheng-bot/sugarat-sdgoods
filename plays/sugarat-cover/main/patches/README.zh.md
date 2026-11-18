---
description: "保留的 LVGL GIF 解码器补丁源码及其本地应用脚本。"
---

# LVGL GIF 解码器补丁

[English](README.md) | 中文

保留的 MIT 许可 `lvgl-8.3.11-gif-psram` 文件与 `components/lvgl/src/extra/libs/gif` 中的本地 GIF 解码器一致。解码器在 PSRAM 中使用 RGB565 缓冲，源数据内存分配成功时持有 GIF 的 RAM 副本。`gifdec.c`、`gifdec.h` 和 `lv_gif.c` 为一组，解码器数据长度参数与图片颜色格式必须保持一致。

`apply_lvgl_patches.py` 逐字节比较源文件与目标文件，复制存在差异的文件，并在校验失败时抛出错误。它操作固定的本地组件，不操作 `managed_components`；当前 CMake 文件不调用该脚本。Sugarat Cover 显示 JPEG 图片，不使用 GIF 播放。再分发这些解码器源码时保留文件内的 MIT 声明。
