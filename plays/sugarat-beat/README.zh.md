---
description: "在 SDGOODS 圆屏徽章上游玩和编译 Sugarat Beat。"
---

# Sugarat Beat

[English](README.md) | 中文

## 概要

Sugarat Beat 是面向 SDGOODS ESP32-S3 圆屏徽章的离线节奏跳跃游戏。关卡跟随 120 BPM、时长 30 秒的原创电子配乐《Kinetic Circuit》推进。两拍倒数后开始闯关，全关共有 18 处障碍。

## 目录

- [玩法](#play)
- [编译](#build)
- [验证](#verification)

<a id="play"></a>
## 玩法

点屏开始，带刺的海胆会自动弹跳。出现 TAP NOW 时，在落地拍点前后点屏，角色就会长跳越过带叉号的缺口。起跳台会亮起绿光，长跳时角色会发出短促的叫声。失误后点一次屏幕即可从头再来。从屏幕顶部下滑可打开控制中心调整音量，关掉控制中心后继续刚才的进度。

<a id="build"></a>
## 编译

启用 ESP-IDF 5.5.4 后，在本目录运行以下命令。构建生成单一应用镜像 `build/sugarat_beat.bin`，配乐已内嵌。修改[配乐脚本](tools/compose.py)后可用 Python 3 重新生成音轨。

```sh
python3 tools/compose.py --wav kinetic-circuit.wav
idf.py -B build build
```

项目基于内置的 [SDGOODS 固件模板](SUGARAT_TEMPLATE.md)。关卡通过 I2S DMA 已传出的音频块计时，即使静音也按音乐时间推进。

<a id="verification"></a>
## 验证

电脑端测试覆盖所有障碍、跳跃失误、重试，并用模拟播放时钟验证了 LVGL 触摸输入。已在 Sugarat 圆屏徽章上核对 v0.1.3 的写入内容。游戏正常进入标题界面，作者试玩并确认了点击跳跃的节奏手感。见 [验证记录](VALIDATION.md)。
