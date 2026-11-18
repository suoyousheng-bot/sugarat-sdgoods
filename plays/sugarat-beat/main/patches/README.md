---
description: "The pinned LVGL GIF memory patches applied during firmware configuration."
---

# LVGL GIF memory patches

English | [中文](README.zh.md)

The imported MIT patches target LVGL 8.3.11. Sugarat Beat draws its game objects with LVGL.

`apply_lvgl_patches.py` runs during CMake configuration, copies `gifdec.c`, `gifdec.h` and `lv_gif.c` into the bundled local LVGL component as one set, and checks exact bytes. Read, copy or verification failures stop configuration. Keep the [upstream license](../../components/lvgl/LICENCE.txt) and file copyright notices with distribution.
