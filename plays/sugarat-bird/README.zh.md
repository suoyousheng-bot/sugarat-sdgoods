---
description: "构建和二开面向 SDGOODS 圆屏徽章的英文口袋小鸟游戏。"
---

# Sugarat 口袋小鸟

[English](README.md) | 中文

## 概述

点击屏幕让小鸟穿过360×360 SDGOODS圆屏徽章上的水管。每通过一根水管得一分，每五分提升难度，最高八级。游戏保存最高9999分的纪录，支持立即重玩，使用英文界面，离线运行。工程使用 ESP-IDF 5.5.4、随附 LVGL 8.3.11和Sugarat Game主题。构建及主机证据记录在 [validation.json](validation.json)；真机行为尚未验证。

## 目录

- [玩法](#play)
- [构建](#build)
- [主机检查](#host-checks)
- [存储与故障](#persistence-and-failures)
- [启动器与设备范围](#launcher-and-device-scope)
- [源码与定制](#sources-and-customization)

<a id="play"></a>
## 玩法

按 `Play` 后，点击屏幕中央拍翅。持续按住只触发一次。碰到水管、顶部或地面结束本局；`Try again` 清零本局分数并保留最高分。飞行区位于(60,120)，大小240×168，英文控件和文字位于圆屏安全矩形内。应用无声，不启动网络或音频服务。

从顶部边缘下滑打开BSP控制中心并暂停飞行，关闭后显示 `Resume` 按钮。屏幕休眠或超过250毫秒的调度间隔也会暂停，需明确恢复，避免返回画面时小鸟继续下落。模拟使用固定8毫秒步长，不依赖绘制频率；游戏中目标为每秒30次画面更新，暂停和准备画面不持续重绘。主机检查不能证明实物面板帧率或触摸延迟。

<a id="build"></a>
## 构建

激活带ESP32-S3工具链的ESP-IDF 5.5.4，在资源包根目录运行：

```sh
idf.py -C plays/sugarat-bird -B /tmp/sugarat-bird-build build
```

应用镜像为 `/tmp/sugarat-bird-build/sugarat_bird.bin`，发布时只提供该应用BIN。保持 `project(sugarat_bird)` 稳定，因为启动器用项目名识别应用。板卡接线、Flash DIO 80 MHz、Octal PSRAM及参考分区表继续由随附BSP管理。构建生成的引导程序和分区镜像不是应用安装资源。

<a id="host-checks"></a>
## 主机检查

准备CMake、Ninja、C编译器和Pillow后运行：

```sh
python3 plays/sugarat-bird/tests/check_host.py --output /tmp/sugarat-bird-host
```

真实模型、存储层、控制器和LVGL界面在AddressSanitizer、UndefinedBehaviorSanitizer及32 KiB LVGL堆下运行。检查覆盖帧率无关性、碰撞、计分、连续45根水管、难度上限、最高分恢复、存储故障、指针输入、浮层/休眠暂停、手动恢复与反复释放画面，并检查最高9999分时的英文字形和文字范围。玩法自有[文字快照](tests/expected/ui-text.txt)覆盖准备、游戏中、暂停、结束、保存失败和最高分画面。命令从真实LVGL界面导出 `preview-en.png`，它不是真机截图。

<a id="persistence-and-failures"></a>
## 存储与故障

专属NVS命名空间 `sugarat_bird` 保存一个 `u32` 类型的 `best` 键。本局结束、打开系统控制或请求正常关机时保存新最高分，提交与读回都成功才确认保存；不会逐帧写入，也不会用较低分数覆盖较高纪录。本局中突然断电可能丢失最近最高分。

存储分数不可读或越界时，本次运行禁止写入，游戏继续并显示 `Best not saved`。写入、提交或读回失败时保留内存中的最高分，显示同一提示，并允许在后续本局结束或正常关机时重试。应用不会通过擦除NVS或格式化存储恢复。主机模拟成功不能证明实物Flash持久性。

<a id="launcher-and-device-scope"></a>
## 启动器与设备范围

随附 `sdgoods_launcher` SDK提供模式检测、控制中心和 `sdgoods_return_to_launcher()`。应用绑定外壳暂停/恢复回调，保留控制中心按模式显示的 `Exit`/`Power` 操作。切换应用通过重启完成；应用代码不写分区表、otadata或固定安装地址。最高分数据不依赖所选应用槽。

本次开发交付不包含串口检查、烧录或实物设备测试。启动器安装、打开、Exit、再次进入保留最高分、与其他应用共存，以及实物显示/输入/电源行为仍需设备验收。获得设备操作授权后，遵循[部署说明](../../../hardware-kits/sugarat-sdgoods/references/deployment.md)。

<a id="sources-and-customization"></a>
## 源码与定制

飞行和水管规则来自Apache-2.0许可的 [SDGOODS Flappy示例](https://github.com/SDGOODS/SDGOODS-ESP32S3/blob/main/main/apps/ui_flappy.c)，[upstream.json](upstream.json)固定所检查的源码。资源包模板清单固定BSP基础。Deotaland维护独立的按时间推进模型、英文圆屏界面、像素小鸟、最高分存储和主机检查。分发应用时保留基础工程许可与声明。

飞行与难度参数位于 `main/bird/bird_model.c`，布局和精灵位于 `main/bird/bird_view.c`，生命周期接入位于 `main/apps/ui_helloapp.c`。界面复用随附Game配色、字体、按钮和边框组件。应用文案为英文，BSP同样固定英文，无需远端服务或单独的资源镜像。
