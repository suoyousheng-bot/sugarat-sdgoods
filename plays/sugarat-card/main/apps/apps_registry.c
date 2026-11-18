/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Single-app entry and polling through the supplied BSP registry. */
#include "apps_registry.h"
#include "sdgoods_board.h"
#include "ui_helloapp.h"
static const sdgoods_app_t apps[]={{.label_zh="社交名片",.label_en="Social Card",.icon="app",.show=ui_helloapp_start,.poll=ui_helloapp_poll}};
const sdgoods_app_t *const g_sdgoods_app=apps;
const int g_sdgoods_app_count=1;
static void poll(void){ui_helloapp_poll();}
void apps_register(void)
{
    sdgoods_apps_set_poll(poll);
    static const sdgoods_nav_t nav={.home_create_show=ui_helloapp_start,.home_show=ui_helloapp_start,.apps_show=ui_helloapp_start};
    sdgoods_ui_set_nav(&nav);
}
