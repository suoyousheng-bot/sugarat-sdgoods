---
description: "Retained LVGL GIF decoder patch sources and their local application script."
---

# LVGL GIF decoder patches

English | [中文](README.zh.md)

The retained MIT-licensed `lvgl-8.3.11-gif-psram` files match the local GIF decoder in `components/lvgl/src/extra/libs/gif`. The decoder uses RGB565 buffers in PSRAM and owns a RAM copy of the GIF source when allocation succeeds. `gifdec.c`, `gifdec.h` and `lv_gif.c` form one set: the decoder's data-size parameter and the image color format must agree.

`apply_lvgl_patches.py` compares each source and destination byte for byte, copies differing files and raises an error if verification fails. It targets the pinned local component rather than `managed_components`; the current CMake files do not invoke it. Sugarat Cover displays JPEG images and does not use GIF playback. Retain the file-level MIT notices when redistributing these decoder sources.
