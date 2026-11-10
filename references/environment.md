# 开发环境

版本由 [resources.lock.json](../resources.lock.json) 固定：ESP-IDF 5.5.4、目标 `esp32s3`、本地 LVGL 8.3.11。SDK、编译器和 Python 环境不在资源包内；初始化固件源码无需联网。

## 已有环境

在已激活的 ESP-IDF 终端运行 `idf.py --version`，确认输出为 ESP-IDF v5.5.4。macOS/Linux 按官方安装方式激活对应 SDK 的 `export.sh`；Windows 使用该版本的 ESP-IDF 命令行环境。仅有其他芯片的工具链不代表已支持 S3，最终以本产品构建结果为准。

资源包辅助脚本仅需 Python 3.9 或更新版本和标准库。ESP-IDF 使用其官方安装器建立的 Python 环境，本机验证使用 Python 3.12。命令中的 `python3` 在 Windows 可替换为该环境的 `python`。

## 缺少环境

使用 [ESP-IDF 5.5.4 的 ESP32-S3 官方安装入口](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32s3/get-started/index.html)，按操作系统选择安装方式。固定版本，不切换到 latest，也不复制 Sugarat 安装目录或开发者的绝对路径。

[Linux/macOS 安装说明](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32s3/get-started/linux-macos-setup.html) 给出系统依赖、`v5.5.4` 递归克隆、`install.sh esp32s3` 和环境激活流程。Windows 按上述入口选择对应安装器。安装需要网络及用户允许的系统工具变更；本资源包没有自动下载脚本。离线完整 SDK 和不同操作系统的全新安装未在本包验证中执行。

## 额外开发资源

编译主题可直接使用随包字体，不需要安装字体生成工具。新增字符或重建图标时才需要 Pillow 及完整 LVGL v8.3.11 的 `scripts/built_in_font` 目录；基础模板裁剪了这个目录。主题的 `tools/make_fonts.py` 和 `tools/make_icons.py` 明确列出输入文件，包含 Source Han Sans、Montserrat、unscii 和 Font Awesome 的相应资源；保留其许可。先确认应用所有文字在字体子集中，运行时传入的任意汉字没有完整字库保证。

本包没有原生预览环境。若要在桌面运行同一应用源码，需要单独实现 Host 适配并准备本地 C/C++ 工具；不能直接运行 ESP32 BIN，也不能以预览代替真机验收。
