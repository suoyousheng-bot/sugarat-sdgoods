# 设备控制案例

[English](README.md) | 中文

在 SDGOODS 360 像素圆屏吧唧上复用已确认预览效果的开关、数值滑块、场景选择器或五向遥控。使用 本资源包入口说明中的独立安装脚本 安装 `sdgoods-case-control`，选择 `target=esp-idf` 与项目语言；依赖会自动包含 Sugarat Watch 和像素角色。将 `sugarat_control` 加入应用 CMake 的 `REQUIRES`。安装保留应用入口和自定义文件，不构建或烧录固件。

## 选择案例与语言

优先使用用户明确要求的语言，否则遵循参考内容语言，再遵循对话语言。选择 `CONTROL_LOCALE_ZH` 或 `CONTROL_LOCALE_EN`；配对的 `control_copy_zh.c` 和 `control_copy_en.c` 资源覆盖全部可见文案。点击标题切换语言，模拟值保持不变。

| 案例 | 行为 |
|---|---|
| `CONTROL_SCENE_SWITCHES` | 独立台灯/风扇开关，初始关闭。 |
| `CONTROL_SCENE_LEVELS` | 亮度和音量滑块，范围 0–100%，初始为 60%/30%。 |
| `CONTROL_SCENE_SCENES` | 阅读：均开启、80%/35%；夜间：台灯开/风扇关、15%/0%；离家：均关闭、0%/0%。手动修改开关或数值会清除所选场景。 |
| `CONTROL_SCENE_REMOTE` | 上/左/确认/右/下、最近操作和上限为 9999 的计数。长按在松手时计数一次，拖出按钮会取消激活。 |

所有页面始终显示本地模拟标记。滑块不控制吧唧的显示亮度或扬声器，遥控按钮不发送红外、BLE、Wi-Fi 或 GPIO 命令。所有值保存在 RAM 中，重新创建屏幕后恢复初始值。关闭模拟设备会保留其数值，修改数值不会将其开启。外部控制需要另行定义命令传输、失败处理和设备确认，模拟值变化不能作为设备确认。

## 接入

产品 BSP 启动后，在 LVGL 线程调用 `control_app_open()`，显式传入语言、初始页面及 `gallery_navigation=false`，创建独立应用。仅在明确需要四页测试总览时开启导航。语言/页面无效或状态内存分配失败时返回 `NULL`。

```c
#include "control_app.h"

lv_obj_t *screen = control_app_open((control_app_config_t){
    .locale = CONTROL_LOCALE_EN,
    .scene = CONTROL_SCENE_SWITCHES,
    .gallery_navigation = false,
});
if (screen) lv_scr_load(screen);
```

屏幕拥有状态和刷新定时器，其 LVGL 用户数据须保留给组件。将 `control_app_set_paused(screen, true/false)` 绑定到已有应用外壳的暂停/恢复回调；暂停会停止刷新并忽略输入，恢复后保留数值。先加载其他屏幕，再调用 `lv_obj_del(screen)`，释放状态和定时器，包括待处理刷新。保留已有 BSP 与应用入口。Sugarat Watch 已包含所需字形，扩展文案时将本组件的 `src` 目录传给其字体生成工具。

## 验证

用户已确认渲染预览，并明确跳过烧录和实物测试。ESP-IDF 5.5.4 固件编译与 LVGL 8.3.11 原生指针检查已通过，仓库的 `examples/device-control` 工程构建同一份组件源码。检查覆盖两种语言、独立与总览模式、32 KiB 堆、圆屏点击区域、限值、场景数值、手动覆盖、导航、长按/取消触摸、暂停恢复、独立实例和完整释放。实际显示、触摸、电源与外部设备控制均未验证，每次硬件集成都需要独立验收。
