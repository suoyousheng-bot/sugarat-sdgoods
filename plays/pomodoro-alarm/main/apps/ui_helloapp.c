/* Modified by Sugarat: see SUGARAT_TEMPLATE.md for source changes. */
/*
 * 谷仓共创计划 · 谷仓 SDGOODS 开放平台基础工程
 * 应用层示例
 * https://github.com/SDGOODS/SDGOODS-ESP32S3
 *
 * Copyright (c) 2026 深圳希德创新网络有限公司 (SDGOODS)
 * SPDX-License-Identifier: Apache-2.0
 *
 * 本文件属于应用层，以 Apache-2.0 发布：可自由商用、可闭源分发，
 * 只需保留本声明并携带 NOTICE 文件。详见 LICENSING.md。
 */

/*
 * 新应用起始骨架（派生自 app_template，已精简为「最小应用」）
 *
 * 本工程是「单应用固件」：开机直接进入唯一的应用（见 apps_registry.c），
 * 屏幕中央默认只显示一行问候语 —— 脚本填的是**清洗后的英文名**：
 *   · 不给名   → "Hello HELLOAPP!"
 *   · 给了 ABC → "Hello ABC!"
 * 中文名只写进工程根的 app_meta.json（市场展示用），**不会**出现在屏上
 * （设备 UI 用纯 ASCII 问候语）。
 *
 * ⚠️ 本工程已按「最小体积」裁剪：**没有中文字库**（cn_font_14/16 已删除），
 *    界面文案请只用 ASCII —— 写中文会显示成方框。想要中文界面，请用
 *    `python3 tools/new_app_project.py <名> --no-trim` 重新派生一份完整工程。
 * 要写真正的应用，照着这个文件改即可。
 *
 * ┌─────────────────────────────────────────────────────────────────────┐
 * │ ⚠️ 圆形屏约束（最重要的一条）                                          │
 * │ 本设备是 360×360 的**圆**屏，四角是被圆边切掉的盲区。所有可见元素——   │
 * │ 文字、图片、按钮、进度条——都必须落在安全矩形内：                       │
 * │     x, y ∈ [SDG_UI_SAFE_X, SDG_UI_SAFE_X + SDG_UI_SAFE_W)            │
 * │     y, h ∈ [SDG_UI_SAFE_Y, SDG_UI_SAFE_Y + SDG_UI_SAFE_H)            │
 * │   （即 x,y ∈ [60,300)、宽高不超 240）。用 sdgoods_ui_in_safe_area()   │
 * │   可程序化判断某个点/控件是否越界。控件设了 border/阴影也要算进尺寸，    │
 * │   否则圆边会把它们切掉（见 sdgoods_ui.h 的说明）。                      │
 * └─────────────────────────────────────────────────────────────────────┘
 *
 * 本最小骨架只演示「建屏 + 居中显示一行问候语 + 接入标准退出/音量外壳」。
 * 写复杂界面（按钮、列表、动画、手势）时，再参考仓库里保留的 app_template.c。
 */

#include "ui_helloapp.h"

#include <stdio.h>

#include "lvgl.h"
#include "esp_log.h"
#include "sdgoods_board.h"   /* 平台层全部能力：屏 / 触摸 / 音频 / 框架 / 栅格常量 */

LV_FONT_DECLARE(si_yuan_black_icon_16);

static const char *TAG = "helloapp";

static lv_obj_t *s_scr;   /* 本应用的屏幕（非 NULL 表示已创建） */

/* ---------------------------------------------------------------------------
 * 退出清理
 *   sdgoods_app_shell 在「已经切到目标屏之后」才回调这里，所以可放心清理：
 *   把 s_scr 置 NULL 即可（屏幕对象交给 LVGL 回收）。
 * ------------------------------------------------------------------------- */
/* ⚠️ 名字别叫 on_exit —— libc 里有同名函数，会报 conflicting types */
static void on_menu_exit(void)
{
    s_scr = NULL;
    ESP_LOGI(TAG, "exit");
}

void ui_helloapp_start(void)
{
    if (s_scr) {
        /* 已创建过：直接切回来，不重建（重建会浪费一次整屏绘制） */
        lv_scr_load(s_scr);
        return;
    }

    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);

    /* 居中问候语（纯 ASCII，无需重跑字体子集）
     * ⚠️ 圆屏安全区：文本居中保证落在安全矩形内；若改大字号，用
     *   sdgoods_ui_in_safe_area() 确认不越界（详见 sdgoods_ui.h）。 */
    lv_obj_t *lbl = lv_label_create(s_scr);
    lv_label_set_text(lbl, "Hello pomodoro_alarm!");
    lv_obj_set_style_text_font(lbl, &si_yuan_black_icon_16, 0);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_set_width(lbl, SDG_UI_SAFE_W);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lbl);

    lv_scr_load(s_scr);

    /* ★★★ 接入标准应用框架（两行，顺序别改）★★★
       bind 必须在 lv_scr_load 之后 —— 它要往当前屏上挂手势捕获层。 */
    sdgoods_app_shell_bind(s_scr);                 /* 顶部下滑出控制中心（设置 / 关于 / 关机或退出） */
    sdgoods_app_shell_set_exit_cb(on_menu_exit);        /* 退出时清理（切屏后才回调） */

    ESP_LOGI(TAG, "start");
}

/* ---------------------------------------------------------------------------
 * 每帧推进
 *   平台主循环每轮（约 2ms）调用一次。最小应用无需逻辑，仅做「不在前台就返回」。
 * ------------------------------------------------------------------------- */
void ui_helloapp_poll(void)
{
    if (!s_scr) {
        return;   /* 不在前台 */
    }
}
