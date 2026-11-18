/* SPDX-License-Identifier: Apache-2.0 */
/* LVGL configuration for recorder desktop tests. */
#ifndef LV_CONF_H
#define LV_CONF_H
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0
#ifdef SNAPSHOT_DEVICE_HEAP
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (32U * 1024U)
#else
#define LV_MEM_CUSTOM 1
#endif
#define LV_MEM_CUSTOM_INCLUDE <stdlib.h>
#define LV_MEM_CUSTOM_ALLOC malloc
#define LV_MEM_CUSTOM_FREE free
#define LV_MEM_CUSTOM_REALLOC realloc
#define LV_TICK_CUSTOM 0
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14
#define LV_USE_LOG 0
#define LV_USE_ASSERT_STYLE 1
#define LV_BUILD_EXAMPLES 0
#endif
