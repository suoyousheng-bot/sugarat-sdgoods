# 时间与效率案例

[English](README.md) | 中文

在谷仓 360 像素圆屏徽章上复用完整的专注与休息计时器、倒计时、秒表或手动时钟。通过 本资源包入口说明中的独立安装脚本 安装 `sdgoods-case-time`，指定 `target=esp-idf` 和项目语言；依赖会包含 `sdgoods-ui-watch` 和像素角色。在应用的 CMake `REQUIRES` 中加入 `sugarat_time`。安装保留已有应用入口和定制文件，不会构建或烧录固件。

## 选择案例与语言

优先使用用户明确要求的语言；否则遵循参考内容的语言，再遵循会话语言。中文选择 `TIME_LOCALE_ZH`，英文选择 `TIME_LOCALE_EN`。`src/time_copy_zh.c` 和 `src/time_copy_en.c` 中的配对资源覆盖控件、滚轮标签、运行状态和完成提示。点击标题切换语言不会重置计时器，但会取消尚未保存的滚轮编辑，并关闭临时提示。

| 案例 | 行为 |
|---|---|
| `TIME_TOOL_FOCUS` | 25/45 分钟专注和 5 分钟休息；开始、暂停、继续、重置；累计完成专注次数，并显式切换专注与休息。 |
| `TIME_TOOL_COUNTDOWN` | 10 秒、5 分钟预设和 00:01–99:59 时长滚轮；拒绝零时长；完成后停在零，等待重新开始。 |
| `TIME_TOOL_STOPWATCH` | 开始、暂停、继续，显示最近一次累计用时及记次次数；暂停时重置。 |
| `TIME_TOOL_CLOCK` | 手动设置小时和分钟，支持跨午夜；没有日历、网络校时或持久化。 |

## 集成

产品 BSP 启动后，在 LVGL 线程调用 `time_app_open()`。提供包含隐藏屏幕期间用时的单调毫秒时钟、语言、所选工具，并指定 `gallery_navigation=false`。这样会生成不带左右总览箭头和页面圆点的独立案例。只有明确要求四工具测试总览时才开启 `gallery_navigation=true`。

```c
#include "time_app.h"
#include "esp_timer.h"

static uint64_t now_ms(void) { return (uint64_t)esp_timer_get_time() / 1000; }

lv_obj_t *screen = time_app_open((time_app_config_t){
    .now_ms = now_ms,
    .locale = TIME_LOCALE_EN,
    .tool = TIME_TOOL_FOCUS,
    .gallery_navigation = false,
});
lv_scr_load(screen);
```

使用工具期间保留页面；`lv_obj_del(screen)` 会释放页面状态和更新定时器。将页面绑定到已有产品应用外壳，加载其他页面后再删除它。专注、倒计时和秒表在总览切页后保留状态。所有状态保存在内存中，重启后重置。完成时仅显示提示，没有声音或唤醒闹钟。隐藏屏幕不会暂停累计计时。`time_model.h` 提供不依赖 LVGL 的相同计时逻辑，供需要其他布局的应用使用。

## 验证

源码来自用户已在谷仓圆屏徽章上验收的时间应用，使用 ESP-IDF 5.5.4 和 LVGL 8.3.11。测试应用在用户验收前通过设备写入、独立摘要校验和启动观察。原生检查覆盖中英文、设备的 32 KiB LVGL 内存池、圆屏按钮边界、暂停与后台计时、自定义时长拒绝路径、专注与休息切换、跨午夜和完整页面释放。仓库的 `examples/time-efficiency` 测试工程直接构建此安装组件。每个新集成仍需独立构建和实物验证；主机渲染不能验证设备上的触摸、电源或提醒行为。
