---
description: "构建、烧录和维护 Sugarat 设备启动器，即承载社区玩法的出厂固件。"
kind: "reference"
---

# Sugarat 启动器

[English](README.md) | 中文

Sugarat 启动器在 SDGOODS ESP32-S3 圆屏徽章上展示已安装的社区玩法，每次启动一个独立固件。应用选择出厂区启动器并重启后返回。启动器使用 10 MiB 应用池和十六个动态大小 OTA 槽位，每槽为图标和发布元数据预留 56 KiB。屏幕、触摸、无线、退出和电源行为均需真机验收。

## 构建

本分支是完整的 ESP-IDF 工程，所有组件都在 `components/` 中，编译时无需下载。每个克隆执行一次 `set-target`。必须使用 ESP-IDF 5.5.4，其他版本未在本设备上验证。工程名必须保持 `SDGOODS_LAUNCHER`，已安装的应用和桌面端依据它识别启动器。

```sh
idf.py set-target esp32s3
idf.py build
```

产物为 `build/SDGOODS_LAUNCHER.bin`，必须放得进 3 MiB 出厂区，编译结束时会显示剩余空间。`idf.py menuconfig` → Sugarat Launcher 可设置社区服务地址、产品和硬件版本 ID，以及串口测试输入。

## 烧录与测试

只向已运行启动器的徽章烧录应用本身，`<port>` 为串口：

```sh
idf.py -p <port> app-flash monitor
```

`app-flash` 只把 `SDGOODS_LAUNCHER.bin` 写入 `0x10000` 的出厂区，保留引导程序、设备上的分区表、日志、已安装玩法、Wi-Fi 设置和应用数据。烧录前先关闭 Sugarat 桌面端等占用串口的程序。

不要对徽章执行 `idf.py flash` 或 `idf.py erase-flash`。`flash` 会同时写入构建分区表，其中只有一个 10 MiB 的 `ota_0`，而不是设备上的十六槽分区表；已安装玩法随即无法识别，桌面端也会拒绝该设备。`erase-flash` 会清空全部内容，包括 Wi-Fi 设置和应用数据。做有风险的测试前，先用桌面端的离线备份做一次完整备份。

没有启动器的徽章需先通过 Sugarat 桌面端初始化一次：以多应用模式安装任意社区玩法。这会写入引导程序、十六槽分区表、首份日志和桌面端自带的启动器；之后再用 `app-flash` 换成自己编译的版本。

开启 `LAUNCHER_SERIAL_TEST_INPUT`（开发默认开启）时，串口接受 `main/launcher_console.c` 中说明的模拟点击、滑动和截图命令。量产版本会关闭此项。

## 修改启动器

新增的中文界面文字需要字体中有对应字形，否则显示为空白。使用 Pillow 和 LVGL v8.3.11 的 `scripts/built_in_font` 目录（例如来自 `git clone --depth 1 -b v8.3.11 https://github.com/lvgl/lvgl`），按启动器源码重新生成：

```sh
cd components/sugarat_watch
python3 tools/make_fonts.py --font-dir <lvgl-v8.3.11>/scripts/built_in_font --sources ../../main
```

修改以 Pull Request 提交到 `launcher` 分支，由维护者审核后合并。请在 PR 中写明真机检查情况：主页、应用列表、下载与安装、卸载、从玩法返回、Wi-Fi 设置，以及安装过程中重启。

## 安装与兼容

Sugarat 桌面端负责 USB 检测、不可变社区版本下载、模式选择、备份入口、覆盖确认、Flash 写入及校验。首次安装启动器会改变分区与系统设置，不能把任意已有固件转换为托管应用；原应用需另有经过接入审查的兼容镜像。

内置标准引导程序不会自动从 A/B 日志恢复分区表。启动器在日志中记录分区更新；桌面会拒绝分区表与日志不一致的状态，中断写入后需检查设备或恢复完整备份，不承诺自动回滚。高地址应用数据保留而不重新格式化；文件系统缺失或损坏由对应固件报告，不会静默擦除。

桌面安装的槽位在末尾 56 KiB 中携带 `SGAPP001` 记录：24 字节头包含魔数、小端 JSON 与 JPEG 长度、两个载荷的 CRC-32 及一个保留字段；UTF-8 JSON 标识内嵌工程、社区玩法、标题与分类，之后是已发布的基线 JPEG。启动器检查长度、CRC 和工程身份后使用元数据。旧 SDGOODS 启动器可能可以运行应用，但不显示此标题与图标；其 `SDGCOVER` 格式独立。社区玩法仍是独立 ESP-IDF 镜像，不是加载至启动器进程的模块。

## 源码与许可

`main/` 包含启动器、槽位分配及日志实现。`components/` 中的 `sdgoods_board`、`sdgoods_launcher`、`lvgl`（8.3.11）和 `jpegenc` 原样取自 `main` 分支 `assets/templates` 中的 SDGOODS 产品模板；`sugarat_watch`、`sugarat_pixel`、`sugarat_wifi` 和 `sugarat_wifi_ui` 取自 `main` 分支的 `assets/components`，其中 `sugarat_watch` 的文字字体按启动器源码重新生成。项目保留上游 [LICENSE](LICENSE)、[NOTICE](NOTICE)、[LICENSING.md](LICENSING.md) 和 [TRADEMARK.md](TRADEMARK.md)。产品文案使用 Sugarat；`SDGOODS_LAUNCHER` 保留为托管应用识别的内嵌身份。
