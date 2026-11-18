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
 * apps_registry.c —— 应用注册表（应用层，单应用直启形态）
 *
 * 这是「应用层 ⇄ 平台层」的唯一接线点。本工程是「单应用固件」：
 * 开机直接进入唯一的应用（ui_helloapp），退出（控制中心返回 / app 自身退出）后
 * 也回到该应用 —— 即这台设备就是一台「helloapp 机」，没有主页 / 演示 / 启动台。
 *
 * 想再加第二个应用（变成「主页 + 多应用」形态）？三步：
 *   1. 复制 main/apps/ui_helloapp.c/.h 成 ui_另一个.c/.h，改名函数 ui_另一个_start/_poll
 *   2. 本文件：加 #include "ui_另一个.h" → 往 s_apps[] 里加一行
 *   3. main/CMakeLists.txt：往 SRCS 里加一行 "apps/ui_另一个.c"
 *   ⚠️ 本工程已按「最小体积」裁剪、**没有中文字库**（cn_font_14/16 已删除，图标字体的
 *      .fallback 已置 NULL）：界面文案请只用 ASCII。要用中文，得先恢复中文字库 +
 *      .fallback，或者直接用 `tools/new_app_project.py <名> --no-trim` 重新派生。
 */

#include "apps_registry.h"

#include <stddef.h>

#include "sdgoods_board.h"   /* 平台层：sdgoods_apps_set_poll / sdgoods_ui_set_nav */
#include "ui_helloapp.h"      /* 本工程唯一应用 */

/* ===========================================================================
 * ★ 应用清单 ★
 *
 * 这一份清单是「应用层 ⇄ 平台层」的接线点：每个应用在这里登记 .poll，
 * 由 apps_poll() 每帧调用；.show 由首屏（开机直入）直接调用（ui_helloapp_start）。
 * 本工程是单应用固件，只有这一款应用。
 *
 *   .label_zh 中文按钮文字 / .label_en 英文按钮文字（按界面语言二选一）
 *   .icon     像素图标键（保留作元数据）
 *   .show     进入应用 / .poll 每帧推进（可为 NULL）
 * =========================================================================== */
static const sdgoods_app_t s_apps[] = {
    { .label_zh = "Voice Recorder", .label_en = "Voice Recorder", .icon = "app",
      .show = ui_helloapp_start, .poll = ui_helloapp_poll },
};

const sdgoods_app_t *const g_sdgoods_app   = s_apps;
const int                  g_sdgoods_app_count = (int)(sizeof(s_apps) / sizeof(s_apps[0]));

/* ---------------------------------------------------------------------------
 * 应用轮询汇总
 *
 * 平台主循环每轮（约 2ms）调用一次。两条硬性要求：
 *   · 每个 *_poll() 必须「自己不是前台就立刻返回」，否则会拖慢整个 UI 刷新；
 *   · 里面**不要**做阻塞操作（vTaskDelay / 等信号量 / 阻塞读串口）。
 *     需要等待的逻辑请放到独立 FreeRTOS 任务里，poll 里只读标志位。
 * ------------------------------------------------------------------------- */
static void apps_poll(void)
{
    for (int i = 0; i < g_sdgoods_app_count; i++) {
        if (s_apps[i].poll) {
            s_apps[i].poll();
        }
    }
}

/* ---------------------------------------------------------------------------
 * 屏幕导航
 * 本工程只有「helloapp」一个应用：首屏 / 回主页 / 应用退出 都直接进该应用。
 * ui_helloapp_start() 内部已含「创建 + 绑定外壳 + 切换」，且有 s_scr 守卫，
 * 重建前会先清理旧屏，不会泄漏屏幕对象。
 * ------------------------------------------------------------------------- */
static void home_create_show(void)
{
    ui_helloapp_start();
}

static void home_show(void)
{
    ui_helloapp_start();
}

static void apps_show(void)
{
    /* 应用退出后回到本应用，而非主页 */
    ui_helloapp_start();
}

void apps_register(void)
{
    sdgoods_apps_set_poll(apps_poll);

    static const sdgoods_nav_t nav = {
        .home_create_show = home_create_show,
        .home_show        = home_show,
        .apps_show        = apps_show,
    };
    sdgoods_ui_set_nav(&nav);
}
