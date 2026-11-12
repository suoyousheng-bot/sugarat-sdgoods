---
description: "构建桌面多应用安装使用的 Sugarat 设备启动器。"
kind: "reference"
---

# Sugarat 启动器

[English](README.md) | 中文

Sugarat 启动器在 SDGOODS ESP32-S3 圆屏徽章上展示已安装的社区玩法，每次启动一个独立固件。应用选择出厂区启动器并重启后返回。启动器使用 10 MiB 应用池和十六个动态大小 OTA 槽位，每槽为图标和发布元数据预留 56 KiB。屏幕、触摸、无线、退出和电源行为均需真机验收。

## 构建

使用现有 ESP-IDF 5.5.4 环境中的 Python、SDK 和工具缓存。输出目录不能已有固件源码：

```sh
python firmware-plays/sugarat-launcher/build.py \
  --sdk /absolute/path/to/esp-idf \
  --tools /absolute/path/to/espressif-tools \
  --output /absolute/path/to/new-build-directory
```

构建器装配内置硬件基础模板及经核对的 UI、Wi-Fi 组件，编译后将启动器和标准 ESP-IDF 引导程序打包至 Hardware Studio，并生成 SHA-256 目录。不打开串口。提交的构建分区表仅提供链接大小上限，桌面安装器负责生成和检查实际十六槽分区表；不要将构建表直接烧录为多应用布局。

## 安装与兼容

[Hardware Studio](../../packages/experimental/hardware-studio/README.md#community-firmware-installation) 负责 USB 检测、不可变社区版本下载、模式选择、备份入口、覆盖确认、Flash 写入及校验。首次安装启动器会改变分区与系统设置，不能把任意已有固件转换为托管应用；原应用需另有经过接入审查的兼容镜像。

内置标准引导程序不会自动从 A/B 日志恢复分区表。启动器在日志中记录分区更新；桌面会拒绝分区表与日志不一致的状态，中断写入后需检查设备或恢复完整备份，不承诺自动回滚。高地址应用数据保留而不重新格式化；文件系统缺失或损坏由对应固件报告，不会静默擦除。

桌面安装的槽位在末尾 56 KiB 中携带 `SGAPP001` 记录：24 字节头包含魔数、小端 JSON 与 JPEG 长度、两个载荷的 CRC-32 及一个保留字段；UTF-8 JSON 标识内嵌工程、社区玩法、标题与分类，之后是已发布的基线 JPEG。启动器检查长度、CRC 和工程身份后使用元数据。旧 SDGOODS 启动器可能可以运行应用，但不显示此标题与图标；其 `SDGCOVER` 格式独立。社区玩法仍是独立 ESP-IDF 镜像，不是加载至启动器进程的模块。

## 源码与许可

`main/` 包含启动器、槽位分配及日志实现。板级支持来自 Hardware Studio 固定的产品模板，UI 和 Wi-Fi 源码来自经过核对的组件库。项目保留上游 [LICENSE](LICENSE)、[NOTICE](NOTICE)、[LICENSING.md](LICENSING.md) 和 [TRADEMARK.md](TRADEMARK.md)。产品文案使用 Sugarat；`SDGOODS_LAUNCHER` 保留为托管应用识别的内嵌身份。
