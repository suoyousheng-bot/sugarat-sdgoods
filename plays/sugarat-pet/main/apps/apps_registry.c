/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Register the standalone pet page with the shared BSP polling and navigation. */
#include "apps_registry.h"
#include "sdgoods_board.h"
#include "ui_helloapp.h"
static const sdgoods_app_t apps[] = {{.label_zh = "Pocket Companion", .label_en = "Pocket Companion", .icon = "app", .show = ui_helloapp_start, .poll = ui_helloapp_poll}};
const sdgoods_app_t *const g_sdgoods_app = apps;
const int g_sdgoods_app_count = 1;
void apps_register(void)
{
    static const sdgoods_nav_t nav = {.home_create_show = ui_helloapp_start, .home_show = ui_helloapp_start, .apps_show = ui_helloapp_start};
    sdgoods_apps_set_poll(ui_helloapp_poll);
    sdgoods_ui_set_nav(&nav);
}
