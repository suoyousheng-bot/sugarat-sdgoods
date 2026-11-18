# 保留的LVGL GIF补丁

[English](README.md) | 中文

随附LVGL 8.3.11的GIF解码器使用PSRAM分配和RGB565画布。这些源码保留上游MIT许可及声明，工程Apache-2.0许可不替代它们。

CMake配置阶段，apply_lvgl_patches.py将gifdec.c、gifdec.h及lv_gif.c与components/lvgl/src/extra/libs/gif内对应文件比较。不同文件会复制后再次比较，读取、复制或校验失败会停止配置。这三个文件共同构成兼容的解码器实现。
