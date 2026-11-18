# Retained LVGL GIF patches

English | [中文](README.zh.md)

The bundled LVGL 8.3.11 GIF decoder uses PSRAM allocation and an RGB565 canvas. These sources retain the upstream MIT license and notices; the project Apache-2.0 license does not replace them.

During CMake configuration, apply_lvgl_patches.py compares gifdec.c, gifdec.h and lv_gif.c against components/lvgl/src/extra/libs/gif. Different files are copied and compared again. Read, copy or comparison failure stops configuration. All three files form one compatible decoder implementation.
