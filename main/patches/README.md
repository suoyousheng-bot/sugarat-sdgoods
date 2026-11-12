# LVGL 8.3.11 GIF patches

English | [中文](README.zh.md)

The launcher uses the pinned local LVGL component supplied by the hardware foundation. During CMake configuration, `apply_lvgl_patches.py` compares and replaces `gifdec.c`, `gifdec.h` and `lv_gif.c` under `components/lvgl/src/extra/libs/gif/`, then verifies every destination byte. Missing files or a failed replacement stop configuration.

The three files form one patch set. The decoder allocates canvas and frame storage in PSRAM, uses RGB565 for the canvas, copies GIF input into RAM, and frees owned input when closing the GIF. `gd_open_gif_data` accepts the source byte length; the LVGL wrapper passes it and declares a true-color image without alpha. Keep the decoder, header and wrapper together when changing this API.

These files retain the upstream MIT license and source notices. The project's Apache-2.0 license does not replace their MIT license. When upgrading LVGL, compare the upstream GIF implementation, update the complete patch set and installer, rebuild, and verify animation colors, memory use and physical playback separately.
