---
description: "Retained LVGL 8.3.11 GIF memory patches and configuration behavior."
---

# LVGL GIF memory patches

English | [中文](README.zh.md)

These imported MIT patches target LVGL 8.3.11. Sugarat Radio uses indexed C images and does not display the upstream boot GIF. The retained decoder patches support other GIF consumers built with this BSP.

`apply_lvgl_patches.py` runs during CMake configuration and copies `gifdec.c`, `gifdec.h` and `lv_gif.c` as one set into the downloaded LVGL component. It compares file bytes, copies differences, and verifies written bytes. Missing destinations produce a warning and are skipped; copy/verification failures fail configuration. Do not assume a warning establishes patch application.

The patches allocate GIF buffers in PSRAM, use RGB565 canvas data, copy memory-backed GIF input to owned RAM, and release that copy on close. All three files must be compatible because decoder declarations, data ownership and LVGL image fields change together. Keep their source and license notices. A new LVGL version needs a separate comparison, build and real-device GIF acceptance; this play pins 8.3.11.
