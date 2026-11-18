# 谷仓 Watch 风格界面组件

[English](README.md) | 中文

`sugarat_watch` 是面向 360x360 谷仓圆屏徽章的 watchOS 风格 LVGL 8.3.11 组件库：黑色背景、圆角卡片、胶囊按钮、抗锯齿中文和 71 个图标。所有应用界面都应由它构建，不要自行给原生 LVGL 控件写样式、绘制图标或添加字体。安装时选择 `sdgoods-ui-watch`、`target=esp-idf`，目录使用固件工程根目录，会同时安装 `sdgoods-pixel-characters`；再在 `main/REQUIRES` 中添加 `sugarat_watch`。`include/sugarat_watch.h` 记录了每个函数，`examples/sw_gallery.c` 用完整界面展示每个组件。

## 选择风格

`sdgoods-ui-game`（`sugarat_game`，前缀 `sg_`）的函数、枚举、图标名和参数顺序与本库相同，外观为复古像素游戏风格，并额外提供 RPG 组件。每个应用只选一种风格。为其中一种写的界面，把 `sw_`/`SW_` 改为 `sg_`/`SG_`、`sugarat_watch.h` 改为 `sugarat_game.h` 并替换 `REQUIRES` 条目即可切换风格；只有 `sw_activity_rings` 没有 Game 对应组件。

## 构建界面

所有函数都须在 `lv_init()` 和 `sw_init()` 之后于 LVGL 线程调用。每个界面从 `sw_screen()` 开始。可滚动内容先创建 `sw_page(screen, title)`，再添加分组标题、卡片、列表项和控件；`sw_scroll_indicator(page)` 添加表冠式位置弧。固定内容用 `lv_obj_align()` 或 `sw_row()`/`sw_column()` 摆放。点击区域和文字应位于 (60,60) 起始的 240x240 安全区域内，组件注明贴边放置的除外，例如 `sw_volume`、`sw_status_bar` 和 `sw_page_dots`。

```c
lv_obj_t *screen = sw_screen();
lv_obj_t *page = sw_page(screen, "设置");
lv_obj_t *wifi = sw_list_item(page, SW_ICON_WIFI, SW_TONE_BLUE, "无线网络", "Sugarat-Lab");
sw_list_item_toggle(wifi, true);
sw_set_action(sw_list_item(page, SW_ICON_BULB, SW_TONE_ORANGE, "亮度与显示", NULL), 1);
lv_obj_add_event_cb(page, on_settings, LV_EVENT_CLICKED, NULL);
lv_obj_add_event_cb(page, on_settings, LV_EVENT_VALUE_CHANGED, NULL);
sw_scroll_indicator(page);
lv_scr_load(screen);
```

额外的文字使用色调（`SW_TONE_*`）和文字角色（`SW_FONT_*`），不要直接指定颜色和字体。文字用 `sw_label` 创建，图标用 `sw_icon` 创建；确需使用原生 LVGL 时，用 `sw_color()` 和 `sw_font()` 设置样式以保持一致。

## 点击与数值

可交互部件发送开启事件冒泡的 `LV_EVENT_CLICKED`，因此在组件、页面或屏幕上注册一个处理器即可收到其下所有点击。用 `sw_set_action(obj, id)` 给按钮、列表项或应用图标标记 0..999 的编号，然后在处理器中读取 `sw_event_action(e)`；它从被点击对象向上查找到处理器所在对象，返回第一个标记，没有则返回 `SW_ACTION_NONE`。内置部件上报 `SW_ACTION_CONFIRM`/`CANCEL`（弹窗）、`PREV`/`PLAY`/`NEXT`（`sw_now_playing`）和 `VOICE`（`sw_voice_button`）；`SW_ACTION_BACK` 留给应用自己的返回按钮。`sw_set_action` 占用对象的 LVGL user data，不要在其中再存放其他指针。开关、滑块、步进器、分段控件和滚轮发送 `LV_EVENT_VALUE_CHANGED`，用头文件中的读取函数或其注明的 LVGL 函数读取数值。

## 组件目录

| 用途 | 函数 |
| --- | --- |
| 基础 | `sw_init`、`sw_screen`、`sw_label`、`sw_icon`/`_set`、`sw_color`、`sw_font`、`sw_set_action`、`sw_event_action` |
| 布局 | `sw_page`、`sw_row`、`sw_column`、`sw_card`、`sw_section`、`sw_status_bar`/`_set_time`/`_set_battery`/`_set_connected`、`sw_page_dots`/`_set`、`sw_scroll_indicator` |
| 控件 | `sw_button`、`sw_icon_button`、`sw_toggle_button`、`sw_toggle`、`sw_checkbox`、`sw_slider`、`sw_stepper`/`_get`/`_set`、`sw_segmented`/`_get`、`sw_progress`/`_set`、`sw_ring`/`_set`、`sw_spinner`、`sw_roller` |
| 列表 | `sw_list_item`、`sw_list_item_value`、`sw_list_item_toggle`、`sw_list_item_chevron` |
| 反馈 | `sw_badge`、`sw_toast`、`sw_notification`、`sw_alert`、`sw_empty_state`、`sw_loading` |
| 数据 | `sw_stat`/`_set`、`sw_gauge`/`_set`、`sw_tile`/`_set`、`sw_bar_chart`/`_set`、`sw_sparkline`/`_set` |
| 时间 | `sw_clock`/`_set`、`sw_analog_clock`/`_set`、`sw_timer`/`_set` |
| 媒体 | `sw_now_playing`/`_set`、`sw_volume`/`_set` |
| AI 陪伴 | `sw_voice_button`/`_set`、`sw_waveform`/`_set_active`、`sw_chat_bubble`、`sw_avatar_frame` |
| 启动器 | `sw_app_grid`、`sw_app_icon` |
| Watch 独有 | `sw_activity_rings`/`_set` |

图标为 `include/sw_icons.h` 中的 `SW_ICON_*`，尺寸 16、20、28 和 40 px（`SW_SIZE_S` 至 `XL`）。需要角色形象时，把 `sugarat_pixel.h` 的 `sp_avatar()` 放进 `sw_avatar_frame()`，并在 `REQUIRES` 中添加 `sugarat_pixel`。

## 示例界面

`examples/sw_gallery.c` 只用公开 API 构建 22 个界面：表盘、指针表盘、活动圆环、应用网格、设置、按钮、选择、数值、滚轮、音乐、计时、运动、通知、弹窗、加载、空状态、控制中心、AI 陪伴、对话、图表、小组件和语音状态。以最接近的界面作为应用界面的起点复制修改。如需运行，将 `examples/sw_gallery.c` 加入应用源文件、`examples/` 加入头文件路径，在 `REQUIRES` 中添加 `sugarat_pixel`，再对 `0..sw_gallery_count()-1` 加载 `sw_gallery_create(index)`。

## 文字与字体

文字字体为 14、16、20、28 像素的 4 位抗锯齿思源黑体子集，数字为 24、40、80 像素的 Montserrat。本应用将 20 像素扩展到基本中日韩统一表意文字区，并作为 28 像素字体的回退；14、16 像素保留常用字和源码字符。不支持表情符号。使用应用的 `tools/prepare_assets.py --font-dir <lvgl>/scripts/built_in_font` 保留这些配置；`tools/make_icons.py` 重新生成固定图标字体。

## 内存与性能

共享中文字库包含时间案例的标签和陪伴角色的名字。新应用文案仍需单独检查字形覆盖或重新生成字体。

应用生成的 20 像素基本中日韩文字字体占用主要 Flash 空间。标签共享字体和色调样式。应用内存与镜像证据见 [validation.json](../../validation.json)。替换屏幕时需要明确对象和动画的所有权；社交名片保留一块屏幕及其面板。

## 许可与验证

代码采用 Apache-2.0。字体子集遵循 `OFL.txt`；Font Awesome 图标设计同时遵循 `FONT-AWESOME-LICENSE.txt`（CC BY 4.0）。组件和示例界面已用 ESP-IDF 5.5.4 在谷仓模板中无警告编译。所有示例界面已在使用 32 KB 堆的主机 LVGL 8.3.11 构建上渲染并人工检查画面；组件尚未在设备上运行。每个新应用都须在设备上验证点击区域、滚动流畅度、字符覆盖和内存。
