---
description: "构建 Sugarat 魔法瓶，在 SDGoods 圆屏徽章上单人游玩或双机互倒。"
---

# Sugarat 魔法瓶

[English](README.md) | 中文

## 概述

魔法瓶将 SDGoods 圆屏徽章变成体感液体玩具。单人模式支持四种颜色和最多600颗粒子的可调液量；两台设备可以在本地配对，让混合液体互相倒来倒去，无需路由器或互联网。设备界面为英文，应用无声。工程使用随附的 ESP-IDF 5.5.4 BSP 和 LVGL 8.3.11，发布一个 `sugarat_fluid.bin`。编译、主机检查和实机证据分别记录在 [validation.json](validation.json)。

## 目录

- [游玩](#play)
- [构建](#build)
- [主机检查](#host-checks)
- [设备与启动器](#device-and-launcher)
- [源码与定制](#sources-and-customization)

<a id="play"></a>
## 游玩

单人模式下，倾斜或摇晃瓶子，轻点屏幕中央切换深海、星光、琥珀和森林，长按中央重置为80%。在屏幕中央向右滑动打开全屏菜单；按钮高60像素并贴合圆形屏幕，触摸范围覆盖按钮之间的空隙。默认80%时，Fill依次切换为100%、空瓶、40%和80%。Color重新着色，Reset恢复为当前颜色的80%。右侧边缘的弧线是液量刻度，每四分之一有一个刻度；液量变化后旁边显示百分比2.5秒，联机时一直显示。重启仅保留颜色设置。

双机游玩步骤：

1. 配对前，一台保留药水，另一台通过Fill切换为空瓶。不同的起始颜色可以混色。
2. 一台右滑，选择Play with friend，再选Host room；另一台选择Find friend，核对房间号后选择Join this room。Next room跳过当前房间。
3. 房主选择Accept friend，双方菜单自动关闭。两台设备先各自直立一次，允许转移。
4. 屏幕顶边就是瓶口，玻璃缺口和外翻的瓶颈标出位置。接水方直立时，浅色箭头向瓶口内流动。另一台倾斜至瓶口低于水平位置开始倒水；琥珀色箭头指向瓶口外表示对方还没接，瓶子仍需先直立一次或已经倒空时会显示提示；倒得越倾斜流速越大。横置或扶正停止，交换双方姿态即可倒回来，无需重新配对。
5. 接满、倒空、双方同时倒水、打开菜单、运动数据过期和系统暂停都会停止后续转移，已经在途的少量液体仍可能结算。联机时可右滑或点屏幕中央打开菜单。

设备由玩家自行摆放，固件不测量相对位置，也不检查物理对齐。倒水方的液体被吸向瓶口，从瓶颈流出，空气泡咕嘟回灌。接水方显示对方颜色的水流从瓶口流入：水流按接水方实测重力下落，越快越细，接近液面时断成水滴并溅起水花。流入的液体只在水流落点加入模拟，保持对方颜色，几秒内逐渐融合为混合色，摇晃会加快融合。不包含声音或多人配方模式。

房主维护两只瓶子的整数液量及四种成分，每次更新收到确认后才允许下一次转移，重传不会重复加水。在液体转移途中，加入方可能短暂显示上一份记录。连续1.5秒未收到有效对端消息时，转移停止并显示Transfer stopped。原来的两台重新靠近并直立后，可在同步后恢复；设备重启需要新建会话。New solo bottle明确结束会话并创建480单位的练习瓶，退出或断电不保存会话液体。邀请在10秒后过期。

附近联机使用ESP-NOW，信道由`CONFIG_BOTTLE_RADIO_CHANNEL`控制，默认6，可在menuconfig配置为1至11；双方必须使用相同信道。建房或查找时才启动Wi-Fi，进入Solo、退出应用或关机时关闭。配对限定对端地址及新房间标识，但没有密码学对端认证或应用数据加密，仅用于本地游玩，不用于保密数据。无线初始化失败会显示Radio unavailable，仍可返回Solo。无线性能、内存余量、续航、物理手势和真正的双机转移需要实机验收。

<a id="build"></a>
## 构建

激活带 ESP32-S3 工具链的 ESP-IDF 5.5.4，在 资源包根目录运行：

```sh
idf.py -C plays/sugarat-fluid -B /tmp/sugarat-fluid-build build
```

应用镜像为 `/tmp/sugarat-fluid-build/sugarat_fluid.bin`。板卡接线、Flash DIO 80 MHz 和 Octal PSRAM 继续由随附 BSP 管理。性能构建保留断言。粒子位置使用内部 RAM，渲染调色表、粒子快照、邻居对缓存和像素帧使用 PSRAM。三个像素缓冲分别保存正在生成、最新完整和正在显示的画面。只有所有权交换持有共享锁，绘制在锁外进行；没有待显示帧时不预占显示机会。没有文字、触摸或系统浮层时，随附的条带发送器直接推屏；浮层使用 LVGL 路径。

<a id="host-checks"></a>
## 主机检查

准备 CMake、Ninja、C 编译器和 Pillow 后运行：

```sh
python3 plays/sugarat-fluid/tests/check_host.py --output /tmp/sugarat-fluid-host
```

帧交接检查覆盖未完成画面、较新画面替换旧画面，以及没有待显示帧时的显示时机。真实模拟器在 AddressSanitizer 和 UndefinedBehaviorSanitizer 下执行2400步变重力与摇晃输入，以及空瓶和满瓶补充序列。两个真实会话引擎通过丢包传输检查双向倒空、成分守恒、容量限制、过期和乱序消息、邀请超时、暂停及重连。真实 LVGL 界面生成主题和所有菜单状态，检查显示字形和标签范围，并比较玩法自有文字快照。主机渲染不能验证物理运动、触摸、电源、面板刷新或实物扬声器声音。

<a id="device-and-launcher"></a>
## 设备与启动器

单应用部署需要按当前设备制定写入方案；构建分区表不能证明已连接设备的布局。启动器安装仅接收应用 BIN，由安装器选择地址。随附的 `sdgoods_launcher` 组件提供 `sdgoods_device_mode.h` 模式检测、`sdgoods_cc.h` 控制中心，以及 `sdgoods_app_sdk.h` 中的 `sdgoods_return_to_launcher()` 返回接口。魔法瓶通过 `sdgoods_app_shell_bind()` 绑定暂停/恢复回调，并使用控制中心按模式显示的 Power/Exit 操作。主题存入专属 NVS 命名空间，目前不调用 `sdgoods_appdata_*`。应用保留这些 BSP 接口；切换应用会重启设备。单应用测试不能证明启动器兼容性。写入前遵循[部署说明](../../../hardware-kits/sugarat-sdgoods/references/deployment.md)。

应用为无声版本，没有音频任务或声音开关。控制中心暂停模拟，并在执行其操作前等待待保存的主题。熄屏时冻结模拟。单人主题在停止切换700毫秒后、启动无线前或打开系统控制时保存，关机也会请求保存；保存前突然断电可能丢失最近主题。专属 `sugarat_fluid` NVS 命名空间保存 `theme`；忽略旧版固件留下的声音设置。存储失败持续显示提示，应用不会通过擦除 NVS 恢复。运动读数不可用或过期时冻结液体，提示重启。

<a id="sources-and-customization"></a>
## 源码与定制

模拟器来自 MIT 许可的 [FluidBox](https://github.com/V4C38/esp32-fluidbox)，版本由 [upstream.json](upstream.json) 固定，并保留其[许可](main/fluid/LICENSE)。圆形容器、渲染器和设备接入由此工程维护。BSP 和 Watch 组件保留各自许可与声明。产品文案使用 Sugarat。

物理系数位于 `main/fluid/config.h`，配色位于 `main/fluid/render.c`，界面位于 `main/apps/bottle_view.c`，液量位于 `main/fluid/liquid.c`，会话规则位于 `main/fluid/link.c`。工作任务拥有模拟和会话，无线回调只入队，LVGL读取发布的快照。加入方替换液量前会校验快照版本和成分总量。添加汉字时按 Watch 文档运行字体生成器。保持 `project(sugarat_fluid)` 不变，让更新继续使用同一应用身份。四套主题通过代码配色，不需要独立资源镜像。
