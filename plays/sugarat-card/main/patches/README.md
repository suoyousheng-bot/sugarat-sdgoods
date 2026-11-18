# Retained LVGL GIF patches

English | [中文](README.zh.md)

These retained sources and `apply_lvgl_patches.py` belong to the supplied SDGOODS template. They carry the upstream MIT license and notices.

The social-card application compiles the local `components/lvgl` tree and uses no GIF animation. Its CMake does not execute this retained patch script. Drawer motion uses LVGL object-position animation.
