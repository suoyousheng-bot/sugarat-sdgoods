/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Register Magic Bottle with the BSP navigation and polling hooks. */
#include "apps_registry.h"
#include "sdgoods_board.h"
#include "ui_helloapp.h"

static const sdgoods_app_t apps[] = {
    {.label_zh = "Magic Bottle", .label_en = "Magic Bottle", .icon = "app", .show = ui_helloapp_start, .poll = ui_helloapp_poll},
};
const sdgoods_app_t *const g_sdgoods_app = apps;
const int g_sdgoods_app_count = 1;

void apps_register(void)
{
    static const sdgoods_nav_t navigation = {
        .home_create_show = ui_helloapp_start,
        .home_show = ui_helloapp_start,
        .apps_show = ui_helloapp_start,
    };
    sdgoods_apps_set_poll(ui_helloapp_poll);
    sdgoods_ui_set_nav(&navigation);
}
