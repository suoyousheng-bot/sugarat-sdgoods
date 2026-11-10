# 谷仓游戏像素风格界面组件

[English](README.md) | 中文

`sugarat_game` 是面向 360x360 谷仓圆屏徽章的复古像素游戏风格 LVGL 8.3.11 组件库：夜蓝背景、带斜面和硬阴影的像素框、方块绘制的圆环与图表、1 位像素中文、71 个像素图标以及 RPG 组件。所有应用界面都应由它构建，不要自行给原生 LVGL 控件写样式、绘制图标或添加字体。安装时选择 `sdgoods-ui-game`、`target=esp-idf`，目录使用固件工程根目录，会同时安装 `sdgoods-pixel-characters`；再在 `main/REQUIRES` 中添加 `sugarat_game`。`include/sugarat_game.h` 记录了每个函数，`examples/sg_gallery.c` 用完整界面展示每个组件。

## 选择风格

`sdgoods-ui-watch`（`sugarat_watch`，前缀 `sw_`）的函数、枚举、图标名和参数顺序与本库相同，外观为 watchOS 风格。每个应用只选一种风格。为其中一种写的界面，把 `sg_`/`SG_` 改为 `sw_`/`SW_`、`sugarat_game.h` 改为 `sugarat_watch.h` 并替换 `REQUIRES` 条目即可切换风格；Game 独有组件和 `sg_box` 没有 Watch 对应组件。

## 构建界面

所有函数都须在 `lv_init()` 和 `sg_init()` 之后于 LVGL 线程调用。每个界面从 `sg_screen()` 开始。可滚动内容先创建 `sg_page(screen, title)`，再添加分组标题、卡片、列表项和控件；`sg_scroll_indicator(page)` 添加位置弧。固定内容用 `lv_obj_align()` 或 `sg_row()`/`sg_column()` 摆放。点击区域和文字应位于 (60,60) 起始的 240x240 安全区域内，组件注明贴边放置的除外，例如 `sg_volume`、`sg_status_bar` 和 `sg_page_dots`。

```c
lv_obj_t *screen = sg_screen();
lv_obj_t *dialog = sg_dialog(screen, "小法师", "固件已编译完成，现在烧录吗？");
lv_obj_align(dialog, LV_ALIGN_BOTTOM_MID, 0, -60);
sp_avatar(sg_dialog_portrait(dialog), SP_CHAR_WIZARD, 2, SP_MOOD_TALK);
static const char *const options[] = {"继续游戏", "新的冒险", "设置"};
lv_obj_align(sg_menu(screen, options, 3, 0), LV_ALIGN_TOP_MID, 0, 70);
lv_obj_add_event_cb(screen, on_tap, LV_EVENT_CLICKED, NULL);
lv_scr_load(screen);
```

额外的文字使用色调（`SG_TONE_*`）和文字角色（`SG_FONT_*`），不要直接指定颜色和字体。文字用 `sg_label` 创建，图标用 `sg_icon` 创建。需要自定义面板时，对一个没有背景、边框和阴影样式的普通对象调用 `sg_box(obj, style, tone, shadow)`；`SG_BOX_PANEL`、`BUTTON`、`FRAME` 和 `INSET` 在 `SG_UNIT`（2 px）网格上绘制斜面，带阴影的框按下时会沉入阴影。尺寸和偏移保持为 `SG_UNIT` 的整数倍，边缘才会清晰。

## 点击与数值

可交互部件发送开启事件冒泡的 `LV_EVENT_CLICKED`，因此在组件、页面或屏幕上注册一个处理器即可收到其下所有点击。用 `sg_set_action(obj, id)` 给按钮、列表项或应用图标标记 0..999 的编号，然后在处理器中读取 `sg_event_action(e)`；它从被点击对象向上查找到处理器所在对象，返回第一个标记，没有则返回 `SG_ACTION_NONE`。内置部件上报 `SG_ACTION_CONFIRM`/`CANCEL`（弹窗）、`PREV`/`PLAY`/`NEXT`（`sg_now_playing`）和 `VOICE`（`sg_voice_button`）；`sg_menu` 选项和 `sg_inventory` 格子上报自身序号。`SG_ACTION_BACK` 留给应用自己的返回按钮。`sg_set_action` 占用对象的 LVGL user data，不要在其中再存放其他指针。开关、滑块、步进器、分段控件和滚轮发送 `LV_EVENT_VALUE_CHANGED`，用头文件中的读取函数或其注明的 LVGL 函数读取数值。

## 组件目录

| 用途 | 函数 |
| --- | --- |
| 基础 | `sg_init`、`sg_screen`、`sg_label`、`sg_icon`/`_set`、`sg_color`、`sg_font`、`sg_box`/`_set_tone`、`sg_set_action`、`sg_event_action` |
| 布局 | `sg_page`、`sg_row`、`sg_column`、`sg_card`、`sg_section`、`sg_status_bar`/`_set_time`/`_set_battery`/`_set_connected`、`sg_page_dots`/`_set`、`sg_scroll_indicator` |
| 控件 | `sg_button`、`sg_icon_button`、`sg_toggle_button`、`sg_toggle`、`sg_checkbox`、`sg_slider`、`sg_stepper`/`_get`/`_set`、`sg_segmented`/`_get`、`sg_progress`/`_set`、`sg_ring`/`_set`、`sg_spinner`、`sg_roller` |
| 列表 | `sg_list_item`、`sg_list_item_value`、`sg_list_item_toggle`、`sg_list_item_chevron` |
| 反馈 | `sg_badge`、`sg_toast`、`sg_notification`、`sg_alert`、`sg_empty_state`、`sg_loading` |
| 数据 | `sg_stat`/`_set`、`sg_gauge`/`_set`、`sg_tile`/`_set`、`sg_bar_chart`/`_set`、`sg_sparkline`/`_set` |
| 时间 | `sg_clock`/`_set`、`sg_analog_clock`/`_set`、`sg_timer`/`_set` |
| 媒体 | `sg_now_playing`/`_set`、`sg_volume`/`_set` |
| AI 陪伴 | `sg_voice_button`/`_set`、`sg_waveform`/`_set_active`、`sg_chat_bubble`、`sg_avatar_frame` |
| 启动器 | `sg_app_grid`、`sg_app_icon` |
| Game 独有 | `sg_hp_bar`/`_set`、`sg_hearts`/`_set`、`sg_dialog`/`_set_text`/`_skip`/`_portrait`、`sg_menu`/`_set_selected`/`_get_selected`、`sg_counter`/`_set`、`sg_level`/`_set`、`sg_inventory`/`_set`/`_select`、`sg_achievement`、`sg_title_screen` |

图标为 `include/sg_icons.h` 中的 `SG_ICON_*`，即 8x8 像素画，尺寸 16、24、32 和 40 px（`SG_SIZE_S` 至 `XL`）。需要角色形象时，把 `sugarat_pixel.h` 的 `sp_avatar()` 放进 `sg_avatar_frame()` 或 `sg_dialog_portrait()`，并在 `REQUIRES` 中添加 `sugarat_pixel`。

## 示例界面

`examples/sg_gallery.c` 只用公开 API 构建 28 个界面：标题画面、状态栏、对话、头像对话、菜单、背包、成就、表盘、指针表盘、应用网格、设置、按钮、选择、数值、滚轮、音乐、计时、通知、弹窗、加载、空状态、控制中心、AI 陪伴、对话气泡、图表、小组件、语音状态和图标集。以最接近的界面作为应用界面的起点复制修改。如需运行，将 `examples/sg_gallery.c` 加入应用源文件、`examples/` 加入头文件路径，在 `REQUIRES` 中添加 `sugarat_pixel`，再对 `0..sg_gallery_count()-1` 加载 `sg_gallery_create(index)`。

## 文字与字体

文字字体为 1 位像素字体：中文取自 Source Han Sans SC，以 16 px 渲染，或以 12 px 渲染后放大 2 倍或 3 倍；拉丁字母和数字取自 unscii-8，并按相同比例放大。字形尺寸为 16 px（说明文字）、24 px（正文、小标题）和 36 px（标题），数字为 32 和 56 px。16 和 24 px 字体包含可打印 ASCII、常用标点、`tools/common-hanzi.txt` 中的 1119 个汉字以及组件源码中出现的字符；36 px 只包含源码中的字符。字体外的字符显示为空白。应用要显示其他字符时，在组件目录运行 `python3 tools/make_fonts.py --font-dir <lvgl>/scripts/built_in_font --sources ../../main`；它需要 Pillow 和 LVGL 8.3 字体目录，从字符串字面量收集非 ASCII 字符并重写 `src/fonts/`。只在运行时出现的文字（如对话回复）仅限已打包的字符。图标像素画位于 `tools/make_icons.py`，该脚本会重新生成图标字体。

## 内存与性能

组件库约占 0.19 MB flash（主要是字体和图标）和 0.3 KB 静态 RAM。标签共享静态的字体和色调样式。在使用设备同款 32 KB LVGL 堆的主机构建上测量，示例界面在 9.1 KB 基线之上占用 0.8-8 KB；背包和设置占用 12-13 KB，71 个图标的测试界面占用 16 KB，峰值为 25.2 KB。只保留一个应用界面：先加载新界面，再用 `lv_scr_load_anim(..., true)` 或 `lv_obj_del()` 删除旧界面。需要同时保留更多界面时，调大 `CONFIG_LV_MEM_SIZE_KILOBYTES` 或把 LVGL 堆放到 PSRAM。打字机、波形、加载动画和闪烁提示的 LVGL 定时器只在对象存在期间运行。

## 许可与验证

代码和像素图标画采用 Apache-2.0。中文字形子集遵循 `OFL.txt`；unscii-8 属于公共领域。组件和示例界面已用 ESP-IDF 5.5.4 在谷仓模板中无警告编译。所有示例界面已在使用 32 KB 堆的主机 LVGL 8.3.11 构建上渲染并人工检查画面；组件尚未在设备上运行。每个新应用都须在设备上验证点击区域、滚动流畅度、字符覆盖和内存。
