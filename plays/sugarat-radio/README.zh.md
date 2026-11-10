---
description: "为 ESP32-S3 圆屏徽章构建和二开面向美国的 Sugarat 电台固件。"
---

# Sugarat 电台

[English](README.md) | 中文

## 概述

Sugarat 电台基于 Apache-2.0 的 [sdgoods-radio](https://github.com/YeatsLiao/sdgoods-radio)，适配资料中定义的 ESP32-S3 360×360 徽章。保留 13 套皮肤和 MP3/AAC 播放，默认英文、手动选择美国地区，并复用 Sugarat 配网组件。行为和可改参数见 [PLAY.md](PLAY.md)，本次验证等级见 [VALIDATION.md](VALIDATION.md)。

## 目录

- [构建](#build)
- [桌面验证](#host-verification)
- [源码与分发](#source-and-distribution)

<a id="build"></a>
## 构建

在 资源包根目录执行以下命令前，激活带 ESP32-S3 工具链的 ESP-IDF 5.5.4。组件管理器使用已提交的锁文件，解析 LVGL 8.3.11 和 Espressif audio codec 2.5.0，无需连接 USB 设备。

```sh
idf.py -C plays/sugarat-radio -B /tmp/sugarat-radio-build build
```

构建目录生成 `sugarat_radio.bin`、bootloader、分区表和 `flash_args`。这是独立 factory 应用；提交的分区表描述预期布局，不代表当前设备布局。不要使用上游平台的 launcher 预编译文件或烧录脚本。部署前需要检查实际目标、Flash 容量和启动布局，通知用户并确认重启与覆盖影响后才能写入。源码开发和编译无需打开串口。

<a id="host-verification"></a>
## 桌面验证

保持 SDK 激活，并准备本机 C 编译器和 Pillow，执行：

```sh
python3 plays/sugarat-radio/tests/check_host.py --output /tmp/sugarat-radio-host
```

脚本编译真实 LVGL、应用和配网 UI，配合确定性的网络、存储、音频替身，对比玩法自有的 [UI 文本快照](tests/expected/ui-text.json)，输出 `sugarat-radio-overview.png`。同时用 SDK 替身执行真实播放器重试、清理和目录解析代码。截图和测试仅验证软件行为，不代表 Wi-Fi 接收、声音、物理触摸或设备启动已验证。仅在审阅有意修改的界面文字和渲染页面后使用 `--record`。

<a id="source-and-distribution"></a>
## 源码与分发

[upstream.json](upstream.json) 固定导入的上游版本。源码携带 [LICENSE](LICENSE)、[NOTICE](NOTICE)、[LICENSING.md](LICENSING.md)、[TRADEMARK.md](TRADEMARK.md) 和字体许可，修改文件标注变更。产品品牌使用 Sugarat；代码标识符和法律归属继续保留上游名称。本工程不包含上游市场发布功能或预编译 launcher。

本工程没有部署精选电台托管服务。默认固件查询美国 Radio Browser 目录，目录失败时使用内置流地址。公开流可访问与获得电台商业产品分发许可是不同事项；开发清单不表示已达成分发协议。没有接入 iHeartRadio、TuneIn、账号或定位服务。
