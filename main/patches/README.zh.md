# LVGL 8.3.11 GIF 补丁

[English](README.md) | 中文

启动器使用硬件基础模板提供的固定版本本地 LVGL 组件。CMake 配置时，`apply_lvgl_patches.py` 比较并替换 `components/lvgl/src/extra/libs/gif/` 中的 `gifdec.c`、`gifdec.h` 和 `lv_gif.c`，再逐字节检查目标文件。缺少文件或替换失败会终止配置。

这三个文件必须作为一组补丁使用。解码器在 PSRAM 分配画布与帧存储，画布使用 RGB565，将 GIF 输入复制到 RAM，并在关闭 GIF 时释放自己持有的输入。`gd_open_gif_data` 接收源数据字节长度，LVGL 包装器传入该长度并声明不带 alpha 的真彩色图像。修改此 API 时需同步保留解码器、头文件与包装器。

这些文件保留上游 MIT 许可和源码声明，项目的 Apache-2.0 许可不替换它们的 MIT 许可。升级 LVGL 时，对照上游 GIF 实现，同步更新完整补丁与安装器，重新构建，并分别验证动画颜色、内存使用和实物播放。
